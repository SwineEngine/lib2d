#ifndef LIB2D_NINE_PATCH_H
#define LIB2D_NINE_PATCH_H

#include <stdint.h>

struct nine_patch;
struct geo_vert;
struct build_params {
    struct l2d_image* image;
    struct geo_vert* geoVerticies;
    unsigned short* geoIndicies;
    float bounds_width;
    float bounds_height;
};

void
nine_patch_build_geo(struct build_params*);

struct nine_patch*
nine_patch_parse(uint8_t* data, int bpp, int width, int height);

#endif
