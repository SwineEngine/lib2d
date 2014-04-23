#ifndef __renderer_H__
#define __renderer_H__

#include "primitives.h"
#include "lib2d.h"

struct vertex {
    float position[4];
    float texCoord[2];
    float misc[4]; // alpha, desaturate, unused, unused
    float color[4];
};

struct attribute {
    l2d_ident name;
    int size;
    float* data;
};

struct batch {
    struct vertex* verticies;
    int vertexCount;
    unsigned short* indicies;
    int indexCount;
    struct attribute* attributes; //stretchy buffer
};

struct sort_cache {
    struct l2d_drawer** buffer;
    int alloc_size;
    int drawer_count;
    bool sort_buffer_dirty;
    bool sort_order_dirty;
};

void
init_sort_cache(struct sort_cache*);

struct l2d_target;
struct mat_cache_entry;
struct ir {
    struct l2d_image_bank* ib;
    struct l2d_target* targetList;
    struct l2d_drawer* drawerList;
    struct sort_cache sort_cache;
    struct l2d_drawer_mask* maskList;
    int viewportWidth, viewportHeight;
    float translate[3];
    struct material* defaultMaterial;
    struct material* premultMaterial;
    struct material* singleChannelDefaultMaterial;
    struct mat_cache_entry* material_cache; // stretchy_buffer

    struct shader** shaderRegistery; // stretchy buffer

    struct vertex* scratchVerticies;
    unsigned short* scratchIndicies;
    struct attribute* scratchAttributes;
};
struct l2d_image;
struct material;

struct ir*
ir_new(struct l2d_image_bank*);

void
ir_delete(struct ir*);

void
ir_render(struct ir*);

struct l2d_drawer;
struct l2d_drawer_mask;

struct l2d_drawer*
l2d_drawer_new(struct ir*);

void
l2d_drawer_delete(struct l2d_drawer*);

void
l2d_drawer_copy(struct l2d_drawer* dst, struct l2d_drawer const* src);

void
l2d_drawer_set_site(struct l2d_drawer*, struct site const*);

const struct site*
l2d_drawer_get_site(struct l2d_drawer* drawer);

void
l2d_drawer_set_clip_site(struct l2d_drawer*, struct site const*);

void
l2d_drawer_set_image(struct l2d_drawer*, struct l2d_image*);

void
l2d_drawer_set_effect(struct l2d_drawer*, struct l2d_effect*);

void
l2d_drawer_set_desaturate(struct l2d_drawer*, float);

void
l2d_drawer_set_color(struct l2d_drawer*, float color[4]);

void
l2d_drawer_setMaterial(struct l2d_drawer*, struct material*);

void
l2d_drawer_set_target(struct l2d_drawer*, struct l2d_target*);

void
l2d_drawer_set_layer(struct l2d_drawer*, int layer);

void
l2d_drawer_setOrder(struct l2d_drawer*, int order);

void
l2d_drawer_add_geo_rect(struct l2d_drawer*, struct rect pos,
        struct rect tex);

struct vert_2d {
    float x, y, u, v;
};
void
l2d_drawer_add_geo_2d(struct l2d_drawer*,
        struct vert_2d* verticies, unsigned int vert_count,
        unsigned int* indicies, unsigned int index_count);

void
l2d_drawer_add_geo_attribute(struct l2d_drawer*,
        l2d_ident attribute,
        unsigned int size, float* verticies, unsigned int vert_count);

void
l2d_drawer_clear_geo(struct l2d_drawer*);

void
l2d_drawer_blend(struct l2d_drawer*, enum l2d_blend);

void
l2d_drawer_set_mask(struct l2d_drawer*, struct l2d_drawer_mask*);

struct l2d_drawer_mask*
l2d_drawer_mask_new(struct ir* ir);

struct l2d_drawer_mask*
l2d_drawer_mask_clone(struct ir* ir, struct l2d_drawer_mask* old);

void
l2d_drawer_mask_set_image(struct l2d_drawer_mask*, struct l2d_image*);

void
l2d_drawer_mask_set_site(struct l2d_drawer_mask*, struct site const*);

void
l2d_drawer_mask_set_alpha(struct l2d_drawer_mask*, float);
#endif

