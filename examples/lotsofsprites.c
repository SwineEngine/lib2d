#define TITLE "lotsofsprites.c"
#include "demo.c"

static
float
r() {
    return rand()/(float)RAND_MAX;
}

void
setup(struct l2d_scene* scene) {
    for (int i=0; i<2400; i++) {
        struct l2d_sprite* s = l2d_sprite_new(scene,
                l2d_ident_from_str("rounded_square.png"), 0);
        l2d_sprite_blend(s, l2d_BLEND_PREMULT);
        l2d_sprite_rgba(s, .5, .2, 1, .2,  0, 0);
        l2d_sprite_rgba(s, 0, .8, 0, .6, 3*r()+2, l2d_ANIM_REVERSE);

        l2d_sprite_xy(s, r()*640, r()*480,  0, 0);
        l2d_sprite_xy(s, r()*640, r()*480, 2, l2d_ANIM_EXTRAPOLATE);
        l2d_sprite_wrap_xy(s, 0, 640, 0, 480);

        l2d_sprite_scale(s, .1,  0, 0);
        l2d_sprite_scale(s, 1, r()+.75f, l2d_ANIM_REVERSE);

        l2d_sprite_rot(s, 360, 2, l2d_ANIM_REPEAT);
    }
}

