#ifndef LIB2D_NINE_PATCH_H
#define LIB2D_NINE_PATCH_H

#include <stdint.h>

struct l2d_nine_patch;
struct geo_vert;
struct build_params {
    struct l2d_image* image;
    struct geo_vert* geoVerticies;
    unsigned short* geoIndicies;
    float bounds_width;
    float bounds_height;
};

void
l2d_nine_patch_build_geo(struct build_params*);

struct l2d_nine_patch*
l2d_nine_patch_parse(uint8_t* data, int bpp, int width, int height);

#endif
