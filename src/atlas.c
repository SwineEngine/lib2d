#include "atlas.h"
#include "stretchy_buffer.h"
#include <stdlib.h>

struct atlas_entry {
    unsigned int w, h, x, y;
    uint8_t* data;
};
static void entry_delete(struct atlas_entry*);

struct atlas {
    unsigned int bpp;
    struct atlas_entry** entries; // stretchy_buffer
    struct atlas_entry** dont_fit; // stretchy_buffer
};

struct atlas*
atlas_new(unsigned int bpp) {
    struct atlas* a = (struct atlas*)malloc(sizeof(struct atlas));
    a->bpp = bpp;
    a->entries = NULL;
    a->dont_fit = NULL;
    return a;
}

struct atlas_entry*
atlas_add_entry(struct atlas* atlas, unsigned int w, unsigned int h,
        uint8_t* data, uint32_t flags) {
    struct atlas_entry* e = (struct atlas_entry*)malloc(sizeof(struct atlas_entry));
    e->w = w;
    e->h = h;

    if (flags) {
        e->w += 2;
        e->h += 2;
    }

    unsigned int bpp = atlas->bpp;
    unsigned int bytes_per_row = atlas->bpp*w;
    if (flags & ATLAS_ENTRY_TRANSPARENT_BORDER) {
        e->data = (uint8_t*)calloc(e->w*e->h, bpp);
    } else {
        e->data = (uint8_t*)malloc(bpp*e->w*e->h);
    }
    if (flags) {
        uint8_t* dest = e->data + (1+e->w)*bpp;
        for (unsigned int i=0; i<h; ++i) {
            // Left border
            if (flags & ATLAS_ENTRY_EXTRUDE_BORDER)
                memcpy(dest + i*e->w*bpp - bpp,
                        data + i*bytes_per_row, bpp);

            // Row
            memcpy(dest + i*e->w*bpp,
                   data + i*bytes_per_row, bytes_per_row);

            // Right border
            if (flags & ATLAS_ENTRY_EXTRUDE_BORDER)
                memcpy(dest + i*e->w*bpp + bytes_per_row,
                        data + i*bytes_per_row + bytes_per_row - bpp, bpp);
        }

        // Top border (including corners)
        if (flags & ATLAS_ENTRY_EXTRUDE_BORDER)
            memcpy(dest - 1*e->w*bpp - bpp,
                   dest - bpp, bytes_per_row + bpp*2);

        // Bottom border (including corners)
        if (flags & ATLAS_ENTRY_EXTRUDE_BORDER)
            memcpy(dest + h*e->w*bpp - bpp,
                   dest + (h-1)*e->w*bpp - bpp, bytes_per_row + bpp*2);
    } else {
        memcpy(e->data, data, atlas->bpp*w*h);
    }

    sbpush(atlas->entries, e);
    return e;
}

void
atlas_remove_entry(struct atlas* atlas, struct atlas_entry* entry) {
    for (int i=0; i<sbcount(atlas->entries); i++) {
        if (atlas->entries[i] == entry) {
            sbremove(atlas->entries, i, 1);
            entry_delete(entry);
            return;
        }
    }
    assert(0);
}

void
atlas_move_entry(struct atlas* dst, struct atlas* src, struct atlas_entry* entry) {
    for (int i=0; i<sbcount(src->entries); i++) {
        if (src->entries[i] == entry) {
            sbremove(src->entries, i, 1);
            break;
        }
    }
    sbpush(dst->entries, entry);
}

static
int
entry_sort(const void* lhs, const void* rhs) {
    return (*(struct atlas_entry**)rhs)->h - (*(struct atlas_entry**)lhs)->h;
}

static uint8_t* build_image(struct atlas*, unsigned int, unsigned int);

uint8_t*
atlas_pack(struct atlas* atlas,
        unsigned int max_width, unsigned int max_height,
        unsigned int* data_w, unsigned int* data_h) {

    sbfree(atlas->dont_fit);
    atlas->dont_fit = NULL;

    struct atlas_entry** entries = atlas->entries;
    qsort(entries, sbcount(entries), sizeof(struct atlas_entry*), entry_sort);

    *data_w = 0;
    *data_h = entries[0]->h;

    uint32_t x=0, y=0;
    uint32_t* previous_row_heights = (uint32_t*)malloc(max_width*2*sizeof(uint32_t));
    uint32_t previous_ptr = 0; // points to the next free item
    uint32_t* this_row_heights = (uint32_t*)malloc(max_width*2*sizeof(uint32_t));
    uint32_t this_ptr = 0; // points to the next free item
#define ADD_THIS_ROW(x, h) (this_row_heights[this_ptr*2]=(x), this_row_heights[this_ptr*2+1]=(h), ++this_ptr)

    struct atlas_entry* used_out_of_place[128];
    unsigned int next_used_out_of_place = 0;

    // First, arrange entries
    for (int i=0; i<sbcount(entries); ++i) {
        struct atlas_entry* e = entries[i];

        if (e->w+x > max_width) {
            // Out of room on this line

            // Search for any entry that will fit the left-over space.
            for (int j=i+1; j<sbcount(entries) && next_used_out_of_place<=128; ++j) {
                struct atlas_entry* t = entries[j];
                // We can ignore height because we know all taller entries were sorted
                // before us.
                if (t->w <= max_width-x) {
                    sbremove(entries, j, 1);
                    j--;
                    used_out_of_place[next_used_out_of_place] = t;
                    next_used_out_of_place++;
                    for (uint32_t k=1; k<previous_ptr; k++) {
                        if (previous_row_heights[k*2] > x) {
                            y = previous_row_heights[(k-1)*2+1];
                            break;
                        }
                    }
                    ADD_THIS_ROW(x, y+t->h);
                    t->x = x;
                    t->y = y;
                    x += t->w;
                    if (max_width-x < 5) break;
                }
            }

            // If we start a new row, make sure this entry can fit in max_height.
            // Find row start (may not be x==0)
            uint32_t x_ = 0;
            uint32_t y_ = this_row_heights[1];
            for (uint32_t j=1; j<this_ptr; j++) {
                if (this_row_heights[(j-1)*2+1] -
                        this_row_heights[j*2+1] >= e->h &&
                        this_row_heights[j*2]+e->w <= max_width) {
                    // Start row here instead of at 0 (assuming our height check passes)
                    x_ = this_row_heights[j*2];
                    y_ = this_row_heights[j*2+1];
                    break;
                }
            }
            if (y_+e->h > *data_h) {
                if (y_+e->h > max_height) {
                    sbpush(atlas->dont_fit, e);
                    sbremove(atlas->entries, i, 1);
                    i--;
                    // This entry can't possibly fit.
                    continue;
                } else {
                    // It fits, but we need to expand our image height.
                    *data_h = y_+e->h;
                }
            }

            // New line
            for (uint32_t j=0; j<previous_ptr; j++) {
                // Inherit remainder space basline of previous row.
                if (previous_row_heights[j*2] > x)
                    ADD_THIS_ROW(previous_row_heights[j*2],
                            previous_row_heights[j*2+1]);
            }

            // Set this row heights to previous.
            uint32_t* t = previous_row_heights;
            previous_row_heights = this_row_heights;
            this_row_heights = t;
            previous_ptr = this_ptr;
            this_ptr = 0;

            // Lastly, make sure our image is wide enough for the previous x.
            if (x > *data_w) *data_w = x;

            y = y_;
            x = x_;
            // If we don't start at the beginning, we need to inherit row
            // heights until our start.
            for (uint32_t j=0; j<previous_ptr; j++) {
                if (previous_row_heights[j*2] < x) {
                    ADD_THIS_ROW(previous_row_heights[j*2],
                            previous_row_heights[j*2+1]);
                } else {
                    break;
                }
            }

        } else {
            // find y - already found if this is the first entry in a row.
            for (uint32_t j=1; j<previous_ptr; j++) {
                if (previous_row_heights[j*2] > x) {
                    y = previous_row_heights[(j-1)*2+1];
                    break;
                }
            }
        }

        ADD_THIS_ROW(x, y+e->h);
        e->x = x;
        e->y = y;

        x += e->w;
    }
    if (x > *data_w) *data_w = x;

    for (unsigned int i=0; i<next_used_out_of_place; i++) {
        sbpush(entries, used_out_of_place[i]);
    }
    
    free(previous_row_heights);
    free(this_row_heights);
    uint8_t* res = build_image(atlas, *data_w, *data_h);
    sbforeachv(struct atlas_entry* e, atlas->dont_fit) {
        sbpush(atlas->entries, e);
    }
    return res;
}

static
uint8_t*
build_image(struct atlas* atlas, unsigned int w, unsigned int h) {
    uint8_t* data = (uint8_t*)malloc(atlas->bpp*w*h);
    sbforeachv(struct atlas_entry* e, atlas->entries) {
        uint8_t* dest = data + (e->x + e->y*w)*atlas->bpp;
        uint32_t bytes_per_row = e->w*atlas->bpp;
        for (unsigned int i=0; i<e->h; ++i) {
            memcpy(dest + i * w * atlas->bpp,
                   e->data + i*bytes_per_row, bytes_per_row);
        }
    }
    return data;
}

struct atlas_entry* const*
atlas_get_pack_failed(struct atlas* atlas, unsigned int* count) {
    if (count)
        *count = sbcount(atlas->dont_fit);
    return atlas->dont_fit;
}

void
atlas_entry_get_packed_location(struct atlas_entry* e,
        unsigned int* x, unsigned int* y,
        unsigned int* w, unsigned int* h) {
    *x = e->x;
    *y = e->y;
    *w = e->w;
    *h = e->h;
}

void
atlas_delete(struct atlas* atlas) {
    sbforeachv(struct atlas_entry* e, atlas->entries) {
        entry_delete(e);
    }
    sbfree(atlas->entries);
    sbfree(atlas->dont_fit);
    free(atlas);
}


void
entry_delete(struct atlas_entry* e) {
    free(e->data);
    free(e);
}

