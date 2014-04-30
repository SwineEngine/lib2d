#include "lib2d.h"
#include "scene.h"
#include "render_api.h"
#include "renderer.h"
#include "image_bank.h"
#include "resources.h"
#include "stretchy_buffer.h"

#include <stdlib.h>
#include <stdio.h>


void
i_sprite_delete(struct l2d_sprite*);

struct l2d_scene*
l2d_scene_new(struct l2d_resources* r) {
    struct l2d_scene* scene = malloc(sizeof(struct l2d_scene));
    scene->ir = ir_new(r->ib);
    scene->res = r;
    scene->sprites = NULL;
    scene->anims_tx = NULL;
    scene->anims_ty = NULL;
    scene->anims_tz = NULL;
    return scene;
}

void
l2d_scene_delete(struct l2d_scene* scene) {
    ir_delete(scene->ir);
    sbforeachv(struct l2d_sprite* s, scene->sprites) {
        i_sprite_delete(s);
    }
    sbfree(scene->sprites);
}

struct l2d_resources*
l2d_scene_get_resources(struct l2d_scene* scene) {
    return scene->res;
}

void
l2d_scene_step(struct l2d_scene* scene, float dt) {
    sbforeachv(struct l2d_sprite* s, scene->sprites) {
        l2d_sprite_step(s, dt);
    }
    l2d_anim_step(&scene->anims_tx, dt, &scene->ir->translate[0]);
    l2d_anim_step(&scene->anims_ty, dt, &scene->ir->translate[1]);
    l2d_anim_step(&scene->anims_tz, dt, &scene->ir->translate[2]);
}

void
l2d_scene_clear(struct l2d_scene* s, uint32_t color) {
    render_api_clear(color);
}

void
l2d_scene_render(struct l2d_scene* s) {
    ib_upload_pending(s->res->ib);
    ir_render(s->ir);
}

void
l2d_scene_set_viewport(struct l2d_scene* scene, int w, int h) {
    scene->ir->viewportWidth = w;
    scene->ir->viewportHeight = h;
}

void
l2d_scene_set_translate(struct l2d_scene* scene, float x, float y, float z, float dt, uint32_t flags) {
    l2d_anim_new(&scene->anims_tx, x, dt, flags);
    l2d_anim_new(&scene->anims_ty, y, dt, flags);
    l2d_anim_new(&scene->anims_tz, z, dt, flags);
}

bool
l2d_scene_feed_click(struct l2d_scene* scene, float x, float y, int button) {
    bool r = false;
    sbforeachv(struct l2d_sprite* s, scene->sprites) {
        r |= l2d_sprite_feed_click(s, x, y, button);
    }
    return r;
}
