#include "target.h"
#include "renderer.h"
#include "image_bank.h"
#include "gl.h"


#include <stdlib.h>

int
i_target_scaled_width(struct l2d_target* t) {
    return (int)(((float)t->width)*t->scaleWidth);
}
int
i_target_scaled_height(struct l2d_target* t) {
    return (int)(((float)t->height)*t->scaleHeight);
}

void
i_prepair_targets_before_texture(struct ir* ir) {
    for (struct l2d_target* itr = ir->targetList; itr != NULL;
            itr=itr->next) {
        if (itr->flags & l2d_TARGET_MATCH_VIEWPORT) {
            itr->width = ir->viewportWidth;
            itr->height = ir->viewportHeight;
        }
        int wantedWidth = i_target_scaled_width(itr);
        int wantedHeight = i_target_scaled_height(itr);
        if (wantedWidth != ib_image_get_width(itr->image)
                || wantedHeight != ib_image_get_height(itr->image)) {
            ib_image_setAsRenderTarget(itr->image, itr, wantedWidth,
                    wantedHeight);
            itr->needsTextureAttached = true;

            if (itr->flags & l2d_TARGET_MANAGE_DRAWER) {
                struct site s;
                site_init(&s);
                s.rect.r = itr->width;
                s.rect.b = itr->height;
                drawer_set_site(itr->drawer, &s);
            }
        }
    }
}

static
void
attachFBOTexture(struct l2d_target* target) {
    // TODO abstract GL
    if (target->fbo == 0) {
        glGenFramebuffers(1, &target->fbo);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, target->fbo);
    if (ib_image_bind_framebuffer_texture(target->image)) {
        target->needsTextureAttached = false;
    }
}

void
i_prepair_targets_after_texture(struct ir* ir) {
    for (struct l2d_target* itr = ir->targetList; itr != NULL;
            itr=itr->next) {
        if (itr->needsTextureAttached) {
            attachFBOTexture(itr);
        }
    }
}

struct l2d_target*
l2d_target_new(struct ir* ir, int width, int height, unsigned int flags) {
    struct l2d_target* target = malloc(sizeof(struct l2d_target));
    target->width = width;
    target->height = height;
    target->flags = flags;
    target->drawerList = NULL;
    init_sort_cache(&target->sort_cache);
    target->fbo = 0;
    target->needsTextureAttached = true;
    target->color[0] = 0.f;
    target->color[1] = 0.f;
    target->color[2] = 0.f;
    target->color[3] = 1.f;

    struct l2d_target** tl = &ir->targetList;
    if (!*tl) {
        *tl = target;
        target->prev = tl;
        target->next = NULL;
    } else {
        struct l2d_target* last = *tl;
        while (last->next)
            last = last->next;
        last->next = target;
        target->prev = &last->next;
        target->next = NULL;
    }

    target->image = ib_image_new(ir->ib);

    if (flags & l2d_TARGET_MANAGE_DRAWER) {
        target->drawer = drawer_new(ir);
        drawer_set_image(target->drawer, target->image);
    } else {
        target->drawer = NULL;
    }

    target->scaleWidth = 1.f;
    target->scaleHeight = 1.f;

    return target;
}

struct l2d_image*
l2d_target_as_image(struct l2d_target* target) {
    return target->image;
}

struct drawer*
l2d_target_as_drawer(struct l2d_target* target) {
    return target->drawer;
}

void
l2d_target_clear_color(struct l2d_target* t, float r, float g, float b,
        float a) {
    t->color[0] = r;
    t->color[1] = g;
    t->color[2] = b;
    t->color[3] = a;
}

void
l2d_target_set_dimensions(struct l2d_target* t, int width, int height) {
    t->width = width;
    t->height = height;
}

void
l2d_target_set_scale(struct l2d_target* t, float scaleWidth,
        float scaleHeight) {
    t->scaleWidth = scaleWidth;
    t->scaleHeight = scaleHeight;
}

