#ifndef TARGET_H
#define TARGET_H

#include "lib2d.h"
#include "renderer.h"
#include <stdint.h>

struct l2d_postprocess;
struct l2d_postprocess_def;

void
i_prepair_targets_before_texture(struct ir* ir);

void
i_prepair_targets_after_texture(struct ir* ir);

struct l2d_target {
    int width, height;
    float scaleWidth, scaleHeight;
    unsigned int flags;
    struct drawer* drawerList;
    struct sort_cache sort_cache;
    bool needsTextureAttached;
    uint32_t fbo;
    struct l2d_image* image;
    struct drawer* drawer;
    float color[4];

    struct l2d_target* next;
    struct l2d_target** prev;
};

int
i_target_scaled_width(struct l2d_target*);
int
i_target_scaled_height(struct l2d_target*);

// If set, the target's dimensions will match the viewport.
static const unsigned int l2d_TARGET_MATCH_VIEWPORT = 1 << 0;
// If set, teh target will maintain a drawer that is the same dimensions as
// itself.
static const unsigned int l2d_TARGET_MANAGE_DRAWER = 1 << 1;

struct l2d_target*
l2d_target_new(struct ir*, int width, int height, unsigned int flags);

struct l2d_image*
l2d_target_as_image(struct l2d_target*);

// Returns the drawer that the target created. Will be null if
// l2d_TARGET_MANAGE_DRAWER isn't set:
struct drawer*
l2d_target_as_drawer(struct l2d_target*);

void
l2d_target_clear_color(struct l2d_target*, float r, float g, float b,
        float a);

void
l2d_target_set_dimensions(struct l2d_target*, int width, int height);

// A scale of '.5' means that the underlying texture has half the resolution
// of the target.
void
l2d_target_set_scale(struct l2d_target*, float scaleWidth, float scaleHeight);


//////////////////////
// postprocess effects
//////////////////////

struct l2d_postprocess*
l2d_postprocess_get(struct ir*, l2d_ident name);

struct l2d_target*
l2d_postprocess_as_target(struct l2d_postprocess*);

struct l2d_postprocess_def*
l2d_postprocess_def_new(l2d_ident name,
        struct l2d_target* (*newCallback)(struct ir*));

#endif
