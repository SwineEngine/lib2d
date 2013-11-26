#ifndef __l2d_scene_H__
#define __l2d_scene_H__

struct l2d_sprite;
struct ir;
struct l2d_image_bank;
struct l2d_resources;
struct l2d_anim;

struct l2d_scene {
    struct ir* ir;
    struct l2d_resources* res;
    struct l2d_sprite** sprites; // stretchy_buffer TODO linked list
    struct l2d_anim* anims_tx, *anims_ty, *anims_tz;
};

#endif
