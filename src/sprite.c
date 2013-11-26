#include "lib2d.h"
#include "scene.h"
#include "renderer.h"
#include "image_bank.h"
#include "resources.h"
#include "stretchy_buffer.h"
#include "primitives.h"
#include <stdlib.h>
#include <stdio.h>

struct l2d_sprite {
    struct l2d_scene* scene;
    struct site site;
    struct drawer* drawer;
    float rot;
    struct l2d_anim* anims_x; // linked list
    struct l2d_anim* anims_y; // linked list
    struct l2d_anim* anims_scale; // linked list
    struct l2d_anim* anims_rot; // linked list
    struct l2d_anim* anims_r, *anims_g, *anims_b, *anims_a; // linked list
    float color[4];
    l2d_event_cb on_click;
    void* on_click_userdata;
    l2d_sprite_cb on_anim_end;
    void* on_anim_end_userdata;
    bool animated_last_step;
};

struct l2d_sprite*
l2d_sprite_new(struct l2d_scene* scene, l2d_ident image, uint32_t flags) {
    struct l2d_sprite* s = malloc(sizeof(struct l2d_sprite));
    s->scene = scene;
    site_init(&s->site);
    s->drawer = drawer_new(scene->ir);
    sbpush(scene->sprites, s);

    struct l2d_image* im = NULL;
    if (image) im = l2d_resources_load_image(scene->res, image);
    if (im) {
        drawer_set_image(s->drawer, im);
        l2d_sprite_set_size(s, ib_image_get_width(im), ib_image_get_height(im),
                flags);
    } else {
        printf("WARNING: No image '%s'\n", l2d_ident_as_char(image));
        im = l2d_resources_load_image(scene->res, l2d_ident_from_str("0xffffffff"));
        drawer_set_image(s->drawer, im);
        l2d_sprite_set_size(s, 64, 64, flags);
    }
    s->rot = 0;

    s->anims_x = NULL;
    s->anims_y = NULL;
    s->anims_scale = NULL;
    s->anims_rot = NULL;
    s->anims_r = NULL;
    s->anims_g = NULL;
    s->anims_b = NULL;
    s->anims_a = NULL;
    s->color[0] = 1;
    s->color[1] = 1;
    s->color[2] = 1;
    s->color[3] = 1;
    s->on_click = NULL;
    s->on_click_userdata = NULL;
    s->on_anim_end = NULL;
    s->on_anim_end_userdata = NULL;
    s->animated_last_step = false;
    return s;
}

void
i_sprite_delete(struct l2d_sprite* s) {
    drawer_delete(s->drawer);
    l2d_sprite_abort_anim(s);
    free(s);
}

void
l2d_sprite_delete(struct l2d_sprite* s) {
    struct l2d_scene* scene = s->scene;
    for(int i=0; i<sbcount(scene->sprites); i++) {
        if (scene->sprites[i] == s) {
            sbremove(scene->sprites, i, 1);
            break;
        }
    }
    i_sprite_delete(s);
}

void
l2d_sprite_set_size(struct l2d_sprite* s, int w, int h, uint32_t flags) {
    s->site.rect.l = -w/2;
    s->site.rect.t = -h/2;
    s->site.rect.r = w/2;
    s->site.rect.b = h/2;
    if (flags & l2d_SPRITE_ANCHOR_LEFT) {
        s->site.rect.l = 0;
        s->site.rect.r = w;
    }
    if (flags & l2d_SPRITE_ANCHOR_TOP) {
        s->site.rect.t = 0;
        s->site.rect.b = h;
    }
    if (flags & l2d_SPRITE_ANCHOR_RIGHT) {
        s->site.rect.l = -w;
        s->site.rect.r = 0;
    }
    if (flags & l2d_SPRITE_ANCHOR_BOTTOM) {
        s->site.rect.t = -h;
        s->site.rect.b = 0;
    }
    drawer_set_site(s->drawer, &s->site);
}

void
l2d_sprite_set_order(struct l2d_sprite* s, int order) {
    drawer_setOrder(s->drawer, order);
}

void
l2d_sprite_set_on_click(struct l2d_sprite* s, l2d_event_cb cb, void* userdata) {
    s->on_click = cb;
    s->on_click_userdata = userdata;
}

void
l2d_sprite_set_on_anim_end(struct l2d_sprite* s, l2d_sprite_cb cb, void* userdata) {
    s->on_anim_end = cb;
    s->on_anim_end_userdata = userdata;
}

void
l2d_sprite_step(struct l2d_sprite* s, float dt) {
    bool u_site = false;
    u_site |= l2d_anim_step(&s->anims_rot, dt, &s->rot);
    if (u_site)
        quaternion_angle_axis(&s->site.quaternion, s->rot, 0, 0, 1);

    u_site |= l2d_anim_step(&s->anims_x, dt, &s->site.x);
    u_site |= l2d_anim_step(&s->anims_y, dt, &s->site.y);
    u_site |= l2d_anim_step(&s->anims_scale, dt, &s->site.scale);
    if (u_site)
        drawer_set_site(s->drawer, &s->site);

    bool u_color = false;
    u_color |= l2d_anim_step(&s->anims_r, dt, &s->color[0]);
    u_color |= l2d_anim_step(&s->anims_g, dt, &s->color[1]);
    u_color |= l2d_anim_step(&s->anims_b, dt, &s->color[2]);
    u_color |= l2d_anim_step(&s->anims_a, dt, &s->color[3]);
    if (u_color)
        drawer_set_color(s->drawer, s->color);

    if (!u_color && !u_site) {
        if (s->on_anim_end && s->animated_last_step) {
            s->on_anim_end(s->on_anim_end_userdata, s);
        }
        s->animated_last_step = false;
    } else {
        s->animated_last_step = true;
    }
}


bool
l2d_sprite_feed_click(struct l2d_sprite* s, float x, float y, int button) {
    if (!s->on_click) return false;
    float out[4];
    if (site_intersect_point(&s->site, x, y, out)) {
        s->on_click(s->on_click_userdata, button, s);
        return true;
    }
    return false;
}

void
l2d_sprite_blend(struct l2d_sprite* s, int enabled) {
    drawer_blend(s->drawer,
        enabled ? BLEND_PREMULT : BLEND_DISABLED);
}

void
l2d_sprite_wrap_xy(struct l2d_sprite* s,
        float x_low, float x_high,
        float y_low, float y_high) {
    if (x_low > x_high) {
        float t = x_high;
        x_high = x_low;
        x_low = t;
    }
    if (y_low > x_high) {
        float t = y_high;
        y_high = y_low;
        y_low = t;
    }
    s->site.wrap[0] = x_low;
    s->site.wrap[1] = x_high;
    s->site.wrap[2] = y_low;
    s->site.wrap[3] = y_high;
}

void
l2d_sprite_xy(struct l2d_sprite* s, float x, float y, float dt, uint32_t flags) {
    l2d_anim_new(&s->anims_x, x, dt, flags);
    l2d_anim_new(&s->anims_y, y, dt, flags);
}

void
l2d_sprite_scale(struct l2d_sprite* s, float scale, float dt, uint32_t flags) {
    l2d_anim_new(&s->anims_scale, scale, dt, flags);
}

void
l2d_sprite_rot(struct l2d_sprite* s, float rot, float dt, uint32_t flags) {
    l2d_anim_new(&s->anims_rot, rot, dt, flags);
}

void
l2d_sprite_a(struct l2d_sprite* s, float a, float dt, uint32_t flags) {
    l2d_anim_new(&s->anims_a, a, dt, flags);
}

void
l2d_sprite_rgb(struct l2d_sprite* s, float r, float g, float b, float dt, uint32_t flags) {
    l2d_anim_new(&s->anims_r, r, dt, flags);
    l2d_anim_new(&s->anims_g, g, dt, flags);
    l2d_anim_new(&s->anims_b, b, dt, flags);
}

void
l2d_sprite_rgba(struct l2d_sprite* s, float r, float g, float b, float a, float dt, uint32_t flags) {
    l2d_sprite_a(s, a, dt, flags);
    l2d_sprite_rgb(s, r, g, b, dt, flags);
}


void
l2d_sprite_abort_anim(struct l2d_sprite* s) {
    l2d_anim_release_all(&s->anims_x);
    l2d_anim_release_all(&s->anims_y);
    l2d_anim_release_all(&s->anims_scale);
    l2d_anim_release_all(&s->anims_rot);
    l2d_anim_release_all(&s->anims_r);
    l2d_anim_release_all(&s->anims_g);
    l2d_anim_release_all(&s->anims_b);
    l2d_anim_release_all(&s->anims_a);
}

