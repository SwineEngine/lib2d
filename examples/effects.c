#define TITLE "effects.c"
#include "demo.c"

void
setup(struct l2d_scene* scene) {
    // Blueprint-style effect
    struct l2d_effect* e = l2d_effect_new();
    float c[9] = {
        1, 1, 1,
        1,-8, 1,
        1, 1, 1};
    l2d_effect_convolve_matrix(e, -1, c);
    // Remember, matrices are column major order.
    float t[16] = {
        -.15, -.15, 0, 0,
        -.3,  -.3,  0, 0,
        -.05, -.05, 0, 0,
        1,1,1,1};
    l2d_effect_color_matrix(e, -1, t);

    struct l2d_sprite* s = l2d_sprite_new(scene,
            l2d_ident_from_str("effect.png"),
            l2d_SPRITE_ANCHOR_LEFT|l2d_SPRITE_ANCHOR_TOP);
    l2d_sprite_set_effect(s, e);
}

