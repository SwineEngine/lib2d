#ifndef __l2d_resources_H__
#define __l2d_resources_H__

#include "lib2d.h"

struct l2d_image;
struct l2d_resources;
struct l2d_image_bank;

struct raw {
    char* bytes;
    int size;
};

typedef struct l2d_image* (*l2d_resources_load_image_func)(void*, const char*, unsigned int);
typedef struct raw* (*l2d_resources_load_raw_func)(void*, const char*);

struct resource_registry {
    l2d_resources_load_image_func load_image;
    l2d_resources_load_raw_func load_raw;
};

struct l2d_resources {
    struct l2d_image_bank* ib;
    void* userdata;
    struct resource_registry registry;
    struct cache_entry* image_cache;
    struct raw_entry* raw_entries;
};

// TODO pass in userdata destructor
struct l2d_resources*
l2d_resources_new(struct l2d_image_bank*, void* userdata, struct resource_registry, const char* raw_manifest);

void
l2d_resources_delete(struct l2d_resources*);

struct l2d_image*
l2d_resources_load_image(struct l2d_resources*, l2d_ident);

struct raw*
l2d_resources_load_raw(struct l2d_resources*, l2d_ident);

void
free_raw(struct raw*);

#endif
