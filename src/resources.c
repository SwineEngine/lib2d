#include "resources.h"
#include "image_bank.h"
#include "scene.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "stretchy_buffer.h"
#include "stb_image.c"

#define EXPORTED __attribute__((visibility("default")))


struct cache_entry {
    l2d_ident key;
    struct l2d_image* image;
};

struct raw_entry {
    l2d_ident key;
    char path[256];
};

struct parse_string {
    const char* c;
    int len;
};

static
void
next_line(struct parse_string* s) {
    const char* c = s->c + s->len;
    while (c[0] == '\n' && c[0] != '\0')
        c++;
    int line_len = 0;
    while (c[line_len] != '\n' && c[line_len] != '\0') {
        line_len += 1;
    }
    s->c = c;
    s->len = line_len;
}
static
struct parse_string
split(struct parse_string* s, char sep) {
    int len = 0;
    while (len < s->len && s->c[len] != sep) {
        len ++;
    }
    assert(s->c[len] == sep);
    s->len -= len+1;
    struct parse_string res = {.c=s->c, .len=len};
    s->c += len+1;
    return res;
}

static
struct l2d_image*
load_image(struct l2d_resources* r, l2d_ident key, const char* path, unsigned int flags) {
    struct l2d_image* im = r->registry.load_image(r->userdata, path, flags);
    if (im == NULL) return NULL;
    struct cache_entry* e = sbadd(r->image_cache, 1);
    e->image = im;
    ib_image_incref(e->image);
    e->key = key;
    return e->image;
}

static
bool
str_ends_with(const char* s, const char* postfix) {
    return strcmp(s+strlen(s)-strlen(postfix), postfix) == 0;
}

EXPORTED
struct l2d_resources*
l2d_resources_new(struct l2d_image_bank* ib, void* userdata,
        struct resource_registry reg,
        const char* raw_manifest) {
    struct l2d_resources* r = (struct l2d_resources*) malloc(
            sizeof(struct l2d_resources));
    r->ib = ib;
    r->userdata = userdata;
    r->registry = reg;
    r->image_cache = NULL;
    r->raw_entries = NULL;

    if (raw_manifest) {
        struct parse_string s = {.c=raw_manifest};
        next_line(&s);
        char key[256];
        char path[256];
        while (s.len) {
            struct parse_string sp = split(&s, ':');

            memcpy(key, sp.c, sp.len);
            key[sp.len] = '\0';

            next_line(&sp);
            memcpy(path, sp.c+1, sp.len);
            path[sp.len-1] = '\0'; // last char was \n

            if (str_ends_with(path, "png") ||
                str_ends_with(path, "jpg")) {
                load_image(r, l2d_ident_from_str(key), path, 0);
            } else {
                struct raw_entry* e = sbadd(r->raw_entries, 1);
                e->key = l2d_ident_from_str(key);
                strcpy(e->path, path);
            }

            next_line(&s);
        }
    }

    return r;
}

EXPORTED
void
l2d_resources_delete(struct l2d_resources* r) {
    // TODO call userdata destructor
    for (int i=0; i<sbcount(r->image_cache); ++i) {
        struct cache_entry* e = &r->image_cache[i];
        ib_image_decref(e->image);
    }
    sbfree(r->image_cache);
    free(r);
}

static
bool
parse_hex(const char* s, uint8_t color[4], enum l2d_image_format* format) {
    if (s[0] != '0' || s[1] != 'x') return false;
    int l = strlen(s+2);
    if (!(l == 2 || l == 6 || l == 8)) return false;

    if (l == 2) *format = l2d_IMAGE_FORMAT_A_8;
    if (l == 6) *format = l2d_IMAGE_FORMAT_RGB_888;
    if (l == 8) *format = l2d_IMAGE_FORMAT_RGBA_8888;

    for (int bi=0; bi<l/2; bi++) {
        const char t[] = {
            *(s+2+bi*2),
            *(s+2+bi*2 + 1),
            '\0'
        };
        uint8_t n = strtol(t, NULL, 16);
        if (n == 0 && (t[0] != '0' || t[1] != '0')) return false;
        color[bi] = n;
    }
    return true;
}

EXPORTED
void
l2d_set_image_data(struct l2d_scene* scene, l2d_ident key,
        int width, int height, enum l2d_image_format format,
        void* data, uint32_t flags) {
    struct l2d_resources* r = scene->res;

    struct l2d_image* im = l2d_resources_load_image(r, key, flags);
    if (!im) {
        im = ib_image_new(r->ib);
        struct cache_entry* e = sbadd(r->image_cache, 1);
        e->image = im;
        ib_image_incref(e->image);
        e->key = key;
    }

    image_set_data(im, width, height, format, data, flags);
}

EXPORTED
struct l2d_image*
l2d_resources_load_image(struct l2d_resources* r, l2d_ident key, uint32_t flags) {
    for (int i=0; i<sbcount(r->image_cache); ++i) {
        struct cache_entry* e = &r->image_cache[i];
        if (e->key == key) {
            return e->image;
        }
    }

    struct l2d_image* image = NULL;

    uint8_t color[4];
    enum l2d_image_format format;
    if (parse_hex(l2d_ident_as_char(key), color, &format)) {
        image = ib_image_new(r->ib);
        image_set_data(image, 1, 1, format, color, 0);

        struct cache_entry* e = sbadd(r->image_cache, 1);
        e->image = image;
        ib_image_incref(image);
        e->key = key;
    } else {
        image = load_image(r, key, l2d_ident_as_char(key), flags);
        if (!image) {
            return NULL;
        }
    }

    return image;
}

EXPORTED
struct raw*
l2d_resources_load_raw(struct l2d_resources* r, l2d_ident key) {
    if (!r->registry.load_raw) return NULL;
    sbforeachp(struct raw_entry* e, r->raw_entries) {
        if (e->key == key)
            return r->registry.load_raw(r->userdata, e->path);
    }
    printf("WARNING: No raw '%s'\n", l2d_ident_as_char(key));
    return NULL;
}


// -------------------------------
// Default resource implementation
// -------------------------------


static
struct l2d_image*
load_image_func(void* userdata, const char* l2d_ident,
        unsigned int flags) {
    struct l2d_image* image = ib_image_new((struct l2d_image_bank*) userdata);

    int x,y,n;
    unsigned char *data = stbi_load(l2d_ident, &x, &y, &n, 0);
    if (!data) {
        return NULL;
    }

    enum l2d_image_format format;
    switch (n) {
    case 1: format = l2d_IMAGE_FORMAT_A_8; break;
    case 3: format = l2d_IMAGE_FORMAT_RGB_888; break;
    case 4: format = l2d_IMAGE_FORMAT_RGBA_8888; break;
    default:
        printf("Unsupported image format when loading %s\n", l2d_ident);
        assert(false); return image;
    }
    image_set_data(image, x, y, format, data,
            flags);
    stbi_image_free(data);
    return image;
}

static
struct raw*
load_raw_func(void* userdata, const char* l2d_ident) {
    struct raw* raw = (struct raw*)malloc(sizeof(struct raw));
    raw->bytes = NULL;
    raw->size = 0;

    FILE* f = fopen(l2d_ident, "rt");
    if (f) {
        fseek(f, 0, SEEK_END);
        raw->size = ftell(f);
        fseek(f, 0, SEEK_SET);
        raw->bytes = malloc(raw->size+1);
        int count = fread(raw->bytes, 1, raw->size, f);
        if (count != raw->size) {
            printf("Size mis-match for %s\n", l2d_ident);
            assert(0);
        } else {
            raw->bytes[raw->size] = '\0';
        }
        fclose(f);
    } else {
        printf("Couldn't find %s\n", l2d_ident);
        assert(0);
    }

    return raw;
}

EXPORTED
struct l2d_resources*
l2d_init_default_resources() {
    static struct l2d_resources* res = NULL;
    if (res) return res;
    struct resource_registry r = {
        .load_image = load_image_func,
        .load_raw   = load_raw_func,
    };
    struct l2d_image_bank* ib = ib_new();
    res = l2d_resources_new(ib, ib, r, NULL);
    return res;
}
