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
    struct drawer** buffer;
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
    struct drawer* drawerList;
    struct sort_cache sort_cache;
    struct drawer_mask* maskList;
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

struct drawer;
struct drawer_mask;

struct drawer*
drawer_new(struct ir*);

void
drawer_delete(struct drawer*);

void
drawer_copy(struct drawer* dst, struct drawer const* src);

void
drawer_set_site(struct drawer*, struct site const*);

void
drawer_set_clip_site(struct drawer*, struct site const*);

void
drawer_set_image(struct drawer*, struct l2d_image*);

void
drawer_set_effect(struct drawer*, struct l2d_effect*);

void
drawer_set_desaturate(struct drawer*, float);

void
drawer_set_color(struct drawer*, float color[4]);

void
drawer_setMaterial(struct drawer*, struct material*);

void
drawer_set_target(struct drawer*, struct l2d_target*);

void
drawer_set_layer(struct drawer*, int layer);

void
drawer_setOrder(struct drawer*, int order);

void
drawer_add_geo_rect(struct drawer*, struct rect pos,
        struct rect tex);

struct vert_2d {
    float x, y, u, v;
};
void
drawer_add_geo_2d(struct drawer*,
        struct vert_2d* verticies, unsigned int vert_count,
        unsigned int* indicies, unsigned int index_count);

void
drawer_add_geo_attribute(struct drawer*,
        l2d_ident attribute,
        unsigned int size, float* verticies, unsigned int vert_count);

void
drawer_clear_geo(struct drawer*);

void
drawer_blend(struct drawer*, enum l2d_blend);

void
drawer_set_mask(struct drawer*, struct drawer_mask*);

struct drawer_mask*
drawer_mask_new(struct ir* ir);

struct drawer_mask*
drawer_mask_clone(struct ir* ir, struct drawer_mask* old);

void
drawer_mask_set_image(struct drawer_mask*, struct l2d_image*);

void
drawer_mask_set_site(struct drawer_mask*, struct site const*);

void
drawer_mask_set_alpha(struct drawer_mask*, float);
#endif

