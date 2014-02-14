#ifndef __lib2d__
#define __lib2d__

#include <stdint.h>
#include <stdbool.h>

const static uint32_t l2d_IMAGE_N_PATCH = 1<<0;
const static uint32_t l2d_IMAGE_NO_ATLAS = 1<<1;
const static uint32_t l2d_IMAGE_NO_CLAMP = 1<<2;
// 3 - 9 reserved for image flags
const static uint32_t l2d_SPRITE_ANCHOR_LEFT = 1<<10;
const static uint32_t l2d_SPRITE_ANCHOR_TOP = 1<<11;
const static uint32_t l2d_SPRITE_ANCHOR_RIGHT = 1<<12;
const static uint32_t l2d_SPRITE_ANCHOR_BOTTOM = 1<<13;


const static uint32_t l2d_ANIM_REPEAT = 1<<0;
const static uint32_t l2d_ANIM_REVERSE = 1<<1;
const static uint32_t l2d_ANIM_EXTRAPOLATE = 1<<2;
const static uint32_t l2d_ANIM_EASE_IN = 1<<3;
const static uint32_t l2d_ANIM_EASE_OUT = 1<<4;


enum l2d_image_format {
    l2d_IMAGE_FORMAT_RGBA_8888,
    l2d_IMAGE_FORMAT_RGB_888,
    l2d_IMAGE_FORMAT_RGB_565,
    l2d_IMAGE_FORMAT_A_8,
};

enum l2d_blend {
    l2d_BLEND_DISABLED,
    l2d_BLEND_DEFAULT,
    l2d_BLEND_PREMULT,
};

struct l2d_ident;
typedef struct l2d_ident* l2d_ident;

l2d_ident
l2d_ident_from_str(const char*);

l2d_ident
l2d_ident_from_strn(const char* str, int len);

const char*
l2d_ident_as_char(l2d_ident);


struct l2d_anim;

void
l2d_anim_new(struct l2d_anim** anim_list, float to_v, float dt, uint32_t flags);

bool
l2d_anim_step(struct l2d_anim** anim_list, float dt, float* dst);

void
l2d_anim_release_all(struct l2d_anim** anim_list);


struct l2d_scene;
struct l2d_resources;

struct l2d_resources*
l2d_init_default_resources();

struct l2d_scene*
l2d_scene_new(struct l2d_resources*);

void
l2d_scene_delete(struct l2d_scene*);

void
l2d_scene_step(struct l2d_scene*, float dt);

void
l2d_scene_clear(struct l2d_scene*, uint32_t color);

void
l2d_scene_render(struct l2d_scene*);

void
l2d_scene_set_viewport(struct l2d_scene*, int w, int h);

void
l2d_scene_set_translate(struct l2d_scene* scene, float x, float y, float z,
        float dt, uint32_t flags);

bool
l2d_scene_feed_click(struct l2d_scene*, float x, float y, int button);




void
l2d_set_image_data(struct l2d_scene*, l2d_ident image,
        int width, int height, enum l2d_image_format,
        void* data, uint32_t image_flags);




/**
 * Effects
 *
 * `input` of -1 will sample from the previously added component. (Or the main
 * texture if no components have been added yet.
 * A value of 0 samples from the main texture. 1 uses the output from the first
 * added component, 2 from the second etc.
 *
 * Matrix values are always column major order.
 */
struct l2d_effect;

struct l2d_effect*
l2d_effect_new();

void
l2d_effect_delete(struct l2d_effect*);

void
l2d_effect_color_matrix(struct l2d_effect*, int input,
        float transform[16]);

void
l2d_effect_fractal_noise(struct l2d_effect*, int input,
        float frequency_x, float frequency_y,
        int octaves, int seed);

void
l2d_effect_convolve_matrix(struct l2d_effect*, int input, float kernel[9]);

void
l2d_effect_erode(struct l2d_effect*, int input);

void
l2d_effect_dilate(struct l2d_effect*, int input);

void
l2d_effect_blur_v(struct l2d_effect*, int input);

void
l2d_effect_blur_h(struct l2d_effect*, int input);




struct l2d_scene;
struct l2d_sprite;

typedef void (*l2d_sprite_cb)(void*, struct l2d_sprite*);
typedef void (*l2d_event_cb)(void*, int button, struct l2d_sprite*);

struct l2d_sprite*
l2d_sprite_new(struct l2d_scene*, l2d_ident image, uint32_t flags);

void
l2d_sprite_delete(struct l2d_sprite*);

void
l2d_sprite_set_parent(struct l2d_sprite*, struct l2d_sprite*);

void
l2d_sprite_set_size(struct l2d_sprite*, int w, int h, uint32_t sprite_flags);

void
l2d_sprite_set_order(struct l2d_sprite*, int order);

void
l2d_sprite_set_on_click(struct l2d_sprite*, l2d_event_cb, void*);

void
l2d_sprite_set_on_anim_end(struct l2d_sprite*, l2d_sprite_cb, void*);

void
l2d_sprite_set_effect(struct l2d_sprite*, struct l2d_effect*);

bool
l2d_sprite_feed_click(struct l2d_sprite*, float x, float y, int button);

void
l2d_sprite_blend(struct l2d_sprite*, enum l2d_blend);

void
l2d_sprite_xy(struct l2d_sprite*, float x, float y, float dt, uint32_t anim_flags);

void
l2d_sprite_wrap_xy(struct l2d_sprite*,
        float x_low, float x_high,
        float y_low, float y_high);

void
l2d_sprite_scale(struct l2d_sprite*, float scale, float dt, uint32_t anim_flags);

void
l2d_sprite_rot(struct l2d_sprite*, float rot, float dt, uint32_t anim_flags);

void
l2d_sprite_a(struct l2d_sprite*, float a, float dt, uint32_t anim_flags);

void
l2d_sprite_rgb(struct l2d_sprite*, float r, float g, float b, float dt, uint32_t anim_flags);

void
l2d_sprite_rgba(struct l2d_sprite*, float r, float g, float b, float a, float dt, uint32_t anim_flags);

void
l2d_sprite_step(struct l2d_sprite*, float dt);

void
l2d_sprite_abort_anim(struct l2d_sprite*);

#endif
