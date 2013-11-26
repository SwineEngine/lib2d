#ifndef __image_bank_H__
#define __image_bank_H__

#include "lib2d.h"
#include "stdbool.h"
#include "stdint.h"


struct texture;
struct l2d_image_bank;
struct l2d_image;

struct l2d_image_bank*
ib_new(void);

void
ib_delete(struct l2d_image_bank*);

void
ib_upload_pending(struct l2d_image_bank*);

void
ib_image_bind(struct l2d_image* image, int pixelSizeUniform, int32_t handle,
        int texture_slot);

void
ib_image_incref(struct l2d_image*);

void
ib_image_decref(struct l2d_image*);

struct rect
ib_image_get_texture_region(struct l2d_image*);

void
ib_image_set_texture(struct l2d_image*, struct texture*);

int
image_sort_compare(struct l2d_image*, struct l2d_image*);

int
ib_image_get_width(struct l2d_image*);

int
ib_image_get_height(struct l2d_image*);


struct l2d_image*
ib_image_new(struct l2d_image_bank*);

void
image_set_data(struct l2d_image*,
        int width, int height, enum l2d_image_format,
        void* data, uint32_t flags);


struct texture*
ib_texture_new(void);

void
ib_texture_incref(struct texture*);

bool
ib_texture_decref(struct texture*);

void
texture_set_image_data(struct l2d_image_bank*, struct texture*, int width, int height,
        enum l2d_image_format, void const* data, bool clamp);

bool
ib_image_same_texture(struct l2d_image* lhs, struct l2d_image* rhs);

struct nine_patch;

void
image_set_nine_patch(struct l2d_image* image,
        struct nine_patch* patch);

struct nine_patch*
image_get_nine_patch(struct l2d_image* image);


enum l2d_image_format
ib_image_format(struct l2d_image*);

#endif
