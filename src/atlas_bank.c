#include "atlas_bank.h"
#include "atlas.h"
#include "stretchy_buffer.h"
#include "primitives.h"
#include <stdlib.h>

static
struct atlas_ref*
create_atlas(struct atlas_bank*, enum l2d_image_format);

static
struct atlas_ref*
get_or_create_atlas(struct atlas_bank*, enum l2d_image_format);

struct atlas_bank_entry {
    struct atlas_entry* atlas_entry;
    struct texture* texture;
    struct rect texture_region;
    uint32_t flags;
};

struct atlas_ref {
    struct atlas* atlas;
    struct texture* texture;
    enum l2d_image_format format;
    bool dirty;
    struct atlas_bank_entry** entries; // stretchy_buffer
}; 

struct atlas_bank {
    struct atlas_ref** atlas_refs; // stretchy_buffer
};

struct atlas_bank*
atlas_bank_new() {
    struct atlas_bank* bank = malloc(sizeof(struct atlas_bank));
    bank->atlas_refs = NULL;
    return bank;
}

void
atlas_bank_delete(struct atlas_bank* bank) {
    // TODO
}

bool
atlas_bank_resolve(struct atlas_bank* bank, struct l2d_image_bank* ib) {
    bool reresolve = false;
    bool found_dirty = false;
    sbforeachv(struct atlas_ref* ref, bank->atlas_refs) {
        if (!ref->dirty) continue;
        found_dirty = true;
        ref->dirty = false;
        unsigned int out_w, out_h;
        uint8_t* data = atlas_pack(ref->atlas, 2048, 2048, &out_w, &out_h);
        texture_set_image_data(ib, ref->texture, out_w, out_h,
                ref->format, data, true);
        free(data);

        struct atlas_entry* const* failed = atlas_get_pack_failed(ref->atlas, NULL);
        if (sbcount(failed)) {
            reresolve = true;
            struct atlas_ref* new_ref = create_atlas(bank, ref->format);
            new_ref->dirty = true;

            sbforeachv(struct atlas_entry* e, failed) {
                atlas_move_entry(new_ref->atlas, ref->atlas, e);
                for (int i=0; i<sbcount(ref->entries); i++) {
                    struct atlas_bank_entry* b_e = ref->entries[i];
                    if (b_e->atlas_entry == e) {
                        sbremove(ref->entries, i, 1);
                        sbpush(new_ref->entries, b_e);
                        break;
                    }
                }
            }
        }

        sbforeachv(struct atlas_bank_entry* b_e, ref->entries) {
            b_e->texture = ref->texture;
            unsigned int x, y, w, h;
            atlas_entry_get_packed_location(b_e->atlas_entry,
                    &x, &y, &w, &h);
            float fx = 1.0/out_w;
            float fy = 1.0/out_h;
            if (b_e->flags) {
                x ++;
                y ++;
                w -= 2;
                h -= 2;
            }
            b_e->texture_region.l = x * fx;
            b_e->texture_region.t = y * fy;
            b_e->texture_region.r = (x+w)*fx;
            b_e->texture_region.b = (y+h)*fy;
        }
    }

    if (reresolve) atlas_bank_resolve(bank, ib);
    return found_dirty;
}

struct atlas_bank_entry*
atlas_bank_new_entry(struct atlas_bank* bank, int width, int height,
        uint8_t* data, enum l2d_image_format format, uint32_t flags) {
    struct atlas_bank_entry* e = malloc(sizeof(struct atlas_bank_entry));
    e->texture = NULL;

    struct atlas_ref* ref = get_or_create_atlas(bank, format);
    ref->dirty = true;
    e->atlas_entry = atlas_add_entry(ref->atlas, width, height, data, flags);
    e->flags = flags;

    sbpush(ref->entries, e);
    return e;
}

struct texture*
atlas_bank_get_texture(struct atlas_bank_entry* e) {
    return e->texture;
}

struct rect
atlas_bank_get_region(struct atlas_bank_entry* e) {
    return e->texture_region;
}


static
struct atlas_ref*
create_atlas(struct atlas_bank* bank, enum l2d_image_format format) {
    int bpp = 0;
    switch (format) {
    case l2d_IMAGE_FORMAT_RGBA_8888: bpp = 4; break;
    case l2d_IMAGE_FORMAT_RGB_888: bpp = 3; break;
    case l2d_IMAGE_FORMAT_RGB_565: bpp = 2; break;
    case l2d_IMAGE_FORMAT_A_8: bpp = 1; break;
    default: assert(false);
    }
    struct atlas_ref* ref = malloc(sizeof(struct atlas_ref));
    sbpush(bank->atlas_refs, ref);
    ref->atlas = atlas_new(bpp);
    ref->texture = ib_texture_new();
    ib_texture_incref(ref->texture);
    ref->format = format;
    ref->entries = NULL;
    return ref;
}

static
struct atlas_ref*
get_or_create_atlas(struct atlas_bank* bank, enum l2d_image_format format) {
    for (int i=sbcount(bank->atlas_refs)-1; i>=0; i--) {
        struct atlas_ref* ref = bank->atlas_refs[i];
        if (ref->format == format)
            return ref;
    }

    return create_atlas(bank, format);
}

