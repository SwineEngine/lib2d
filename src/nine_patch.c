#include "nine_patch.h"
#include "image_bank.h"
#include "stretchy_buffer.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

static const unsigned short g3x3Indices[] = {
    0, 5, 1,    0, 4, 5,
    1, 6, 2,    1, 5, 6,
    2, 7, 3,    2, 6, 7,
    
    4, 9, 5,    4, 8, 9,
    5, 10, 6,   5, 9, 10,
    6, 11, 7,   6, 10, 11,
    
    8, 13, 9,   8, 12, 13,
    9, 14, 10,  9, 13, 14,
    10, 15, 11, 10, 14, 15
};

struct l2d_nine_patch {
    int8_t wasDeserialized;
    uint8_t numXDivs;
    uint8_t numYDivs;
    uint8_t numColors;
    int32_t* xDivs;
    int32_t* yDivs;
    int32_t paddingLeft, paddingRight;
    int32_t paddingTop, paddingBottom;
    uint32_t* colors;
};

void
l2d_nine_patch_deserialize(const void* inData, struct l2d_nine_patch* outData) {
    char* patch = (char*) inData;
    if (inData != outData) {
        // copy  wasDeserialized, numXDivs, numYDivs, numColors
        memcpy(&outData->numXDivs, patch, 4);
        // copy padding
        memcpy(&outData->paddingLeft, patch + 12, 32);
    }
    char* data = (char*)outData;
    data += sizeof(struct l2d_nine_patch);
    outData->xDivs = (int32_t*) data;

    data += outData->numXDivs * sizeof(int32_t);
    outData->yDivs = (int32_t*) data;

    data += outData->numYDivs * sizeof(int32_t);
    outData->colors = (uint32_t*) data;
}

struct geo_vert {
    float x,y,u,v;
};

static
void
fillIndices(uint16_t indices[], int xCount, int yCount) {
    int n = 0;
    for (int y = 0; y < yCount; y++) {
        for (int x = 0; x < xCount; x++) {
            *indices++ = n;
            *indices++ = n + xCount + 2;
            *indices++ = n + 1;
            
            *indices++ = n;
            *indices++ = n + xCount + 1;
            *indices++ = n + xCount + 2;
            
            n += 1;
        }
        n += 1;
    }
}

static
float
computeVertexDelta(bool isStretchyVertex, float currentVertex,
        float prevVertex, float stretchFactor) {
    // the standard delta between vertices if no stretching is required
    float delta = currentVertex - prevVertex;

    // if the stretch factor is negative or zero we need to shrink the 9-patch
    // to fit within the target bounds.  This means that we will eliminate all
    // stretchy areas and scale the fixed areas to fit within the target bounds.
    if (stretchFactor <= 0) {
        if (isStretchyVertex)
            delta = 0; // collapse stretchable areas
        else
            delta = delta * -stretchFactor; // scale fixed areas
    // if the stretch factor is positive then we use the standard delta for
    // fixed and scale the stretchable areas to fill the target bounds.
    } else if (isStretchyVertex) {
        delta = delta * stretchFactor;
    }

    return delta;
}

static
void
fillRow(struct geo_vert* verts,
        const float vy, const float ty,
        const int bounds_width, const int32_t xDivs[], int numXDivs,
        const float stretchX, int width) {
#define SET(X, Y, U, V) \
    verts->x = (float)X;\
    verts->y = (float)Y;\
    verts->u = (float)U;\
    verts->v = (float)V;\
    verts++
    float vx = 0;
    SET(vx, vy, 0.f, ty);

    float prev = 0;
    for (int x = 0; x < numXDivs; x++) {
        const float tx = xDivs[x];
        vx += computeVertexDelta(x & 1, tx, prev, stretchX);
        prev = tx;

        SET(vx, vy, tx/width, ty);
    }
    SET(bounds_width, vy, 1.f, ty);
#undef SET
}


void
l2d_nine_patch_build_geo(struct build_params* params) {
    struct l2d_nine_patch* patch = l2d_image_get_nine_patch(params->image);
    float width = ib_image_get_width(params->image);
    float height = ib_image_get_height(params->image);
    
    const unsigned int numXStretch = (patch->numXDivs + 1) >> 1;
    const unsigned int numYStretch = (patch->numYDivs + 1) >> 1;
    
    if (numXStretch < 1 && numYStretch < 1) {
        return;
    }
    
    float stretchX = 0, stretchY = 0;
    
    if (numXStretch > 0) {
        unsigned int stretchSize = 0;
        for (int i = 1; i < patch->numXDivs; i += 2) {
            stretchSize += patch->xDivs[i] - patch->xDivs[i-1];
        }
        const float fixed = width - stretchSize;
        if (params->bounds_width >= fixed)
            stretchX = (params->bounds_width - fixed) / stretchSize;
        else // reuse stretchX, but keep it negative as a signal
            stretchX = -params->bounds_width / fixed;
    }
    
    if (numYStretch > 0) {
        unsigned int stretchSize = 0;
        for (int i = 1; i < patch->numYDivs; i += 2) {
            stretchSize += patch->yDivs[i] - patch->yDivs[i-1];
        }
        const float fixed = height - stretchSize;
        if (params->bounds_height >= fixed)
            stretchY = (params->bounds_height - fixed) / stretchSize;
        else // reuse stretchY, but keep it negative as a signal
            stretchY = -params->bounds_height / fixed;
    }

    const unsigned int vCount = (patch->numXDivs+2) * (patch->numYDivs+2);
    // number of cells * 2 (tris per cell) * 3 (verts per tri)
    const unsigned int
    indexCount = (patch->numXDivs+1) * (patch->numYDivs+1) * 2 * 3;

    struct geo_vert* v = sbadd(params->geoVerticies, vCount);
    unsigned short* ind = sbadd(params->geoIndicies, indexCount);


    // we use <= for YDivs, since the prebuild indices work for 3x2 and 3x1 too
    if (patch->numXDivs == 2 && patch->numYDivs <= 2) {
        memcpy(ind, g3x3Indices, indexCount*sizeof(unsigned short));
    } else {
        fillIndices(ind, patch->numXDivs + 1, patch->numYDivs + 1);
    }
    

    float prev = 0;
    float vy = 0;
    fillRow(v, vy, 0, params->bounds_width, patch->xDivs, patch->numXDivs,
            stretchX, width);
    v += patch->numXDivs + 2;
    for (int y = 0; y < patch->numYDivs; y++) {
        const float ty = patch->yDivs[y];
        vy += computeVertexDelta(y & 1, ty, prev, stretchY);
        prev = ty;
        fillRow(v, vy, ty/height, params->bounds_width, patch->xDivs, patch->numXDivs,
                stretchX, width);
        v += patch->numXDivs + 2;
    }
    fillRow(v, params->bounds_height, 1, params->bounds_width,
            patch->xDivs, patch->numXDivs, stretchX, width);
}


enum {
    TICK_TYPE_NONE,
    TICK_TYPE_TICK,
    TICK_TYPE_LAYOUT_BOUNDS,
    TICK_TYPE_BOTH
};

static
int
tick_type(uint8_t* p, int bpp, const char** outError) {
    // TODO make sure this works for non RGBA images
    uint32_t color = p[0];
    if (bpp >= 2) color |= (p[1] << 8);
    if (bpp >= 3) color |= (p[2] << 16);
    if (bpp == 4) color |= (p[3] << 24);

    uint32_t COLOR_WHITE = 0xFF;
    if (bpp >= 2) COLOR_WHITE |= (0xFF << 8);
    if (bpp >= 3) COLOR_WHITE |= (0xFF << 16);
    if (bpp == 4) COLOR_WHITE |= (0xFF << 24);

    uint32_t COLOR_TICK = (bpp == 4) ? 0xFF000000 : 0x0;
    uint32_t COLOR_LAYOUT_BOUNDS_TICK = (bpp == 4) ? 0xFF0000FF : 0xFF;

    if (bpp == 4) {
        if (p[3] == 0) {
            return TICK_TYPE_NONE;
        }
        if (color == COLOR_LAYOUT_BOUNDS_TICK) {
            return TICK_TYPE_LAYOUT_BOUNDS;
        }
        if (color == COLOR_TICK) {
            return TICK_TYPE_TICK;
        }

        // Error cases
        if (p[3] != 0xff) {
            *outError = "Frame pixels must be either solid or transparent (not intermediate alphas)";
            return TICK_TYPE_NONE;
        }
        if (p[0] != 0 || p[1] != 0 || p[2] != 0) {
            *outError = "Ticks in transparent frame must be black or red";
        }
        return TICK_TYPE_TICK;
    } else {
        if (color == COLOR_WHITE) {
            return TICK_TYPE_NONE;
        }
        if (color == COLOR_TICK) {
            return TICK_TYPE_TICK;
        }
        if (color == COLOR_LAYOUT_BOUNDS_TICK) {
            return TICK_TYPE_LAYOUT_BOUNDS;
        }

        if (p[0] != 0) {
            *outError = "Ticks in white frame must be black or red";
            return TICK_TYPE_NONE;
        }
        return TICK_TYPE_TICK;
    }
}

enum {
    TICK_START,
    TICK_INSIDE_1,
    TICK_OUTSIDE_1
};

static int get_horizontal_ticks(
        uint8_t* row, int width, int bpp, bool required,
        int32_t* outLeft, int32_t* outRight, const char** outError,
        uint8_t* outDivs, bool multipleAllowed)
{
    int i;
    *outLeft = *outRight = -1;
    int state = TICK_START;
    bool found = false;

    for (i=1; i<width-1; i++) {
        if (TICK_TYPE_TICK == tick_type(row+i*bpp, bpp, outError)) {
            if (state == TICK_START ||
                (state == TICK_OUTSIDE_1 && multipleAllowed)) {
                *outLeft = i-1;
                *outRight = width-2;
                found = true;
                if (outDivs != NULL) {
                    *outDivs += 2;
                }
                state = TICK_INSIDE_1;
            } else if (state == TICK_OUTSIDE_1) {
                *outError = "Can't have more than one marked region along edge";
                *outLeft = i;
                return 1;
            }
        } else if (*outError == NULL) {
            if (state == TICK_INSIDE_1) {
                // We're done with this div.  Move on to the next.
                *outRight = i-1;
                outRight += 2;
                outLeft += 2;
                state = TICK_OUTSIDE_1;
            }
        } else {
            *outLeft = i;
            return 1;
        }
    }

    if (required && !found) {
        *outError = "No marked region found along horizontal edge";
        *outLeft = -1;
        return 1;
    }

    return 0;
}

static int get_vertical_ticks(
        uint8_t* p, int offset, int width, int height, int bpp,
        bool required, int32_t* outTop, int32_t* outBottom,
        const char** outError, uint8_t* outDivs, bool multipleAllowed)
{
    int i;
    *outTop = *outBottom = -1;
    int state = TICK_START;
    bool found = false;

    for (i=1; i<height-1; i++) {
        if (TICK_TYPE_TICK == tick_type(p+i*width*bpp+offset, bpp, outError)) {
            if (state == TICK_START ||
                (state == TICK_OUTSIDE_1 && multipleAllowed)) {
                *outTop = i-1;
                *outBottom = height-2;
                found = true;
                if (outDivs != NULL) {
                    *outDivs += 2;
                }
                state = TICK_INSIDE_1;
            } else if (state == TICK_OUTSIDE_1) {
                *outError = "Can't have more than one marked region along edge";
                *outTop = i;
                return 1;
            }
        } else if (*outError == NULL) {
            if (state == TICK_INSIDE_1) {
                // We're done with this div.  Move on to the next.
                *outBottom = i-1;
                outTop += 2;
                outBottom += 2;
                state = TICK_OUTSIDE_1;
            }
        } else {
            *outTop = i;
            return 1;
        }
    }

    if (required && !found) {
        *outError = "No marked region found along vertical edge";
        *outTop = -1;
        return 1;
    }
    return 0;
}


struct l2d_nine_patch*
l2d_nine_patch_parse(uint8_t* p, int bpp, int width, int height) {
    struct l2d_nine_patch* patch = (struct l2d_nine_patch*)malloc(
            sizeof(struct l2d_nine_patch));
    patch->numXDivs = 0;
    patch->numYDivs = 0;
    patch->paddingLeft = 0;
    patch->paddingTop = 0;
    patch->paddingRight = 0;
    patch->paddingBottom = 0;
    patch->colors = NULL;
    patch->xDivs = NULL;
    patch->yDivs = NULL;

    int32_t tmpDivs[width>height?width:height];

    const char *errorMsg = NULL;
    // Find left and right of sizing areas...
    if (get_horizontal_ticks(p, width, bpp, false, &tmpDivs[0],
            &tmpDivs[1], &errorMsg, &patch->numXDivs, true) != 0) {
        printf("%s\n", errorMsg);
        return NULL;
    }
    if (patch->numXDivs) {
        patch->xDivs = (int32_t*)malloc(patch->numXDivs*sizeof(int32_t));
        memcpy(patch->xDivs, tmpDivs, patch->numXDivs*sizeof(int32_t));
    }

    // Find top and bottom of sizing areas...
    if (get_vertical_ticks(p, 0, width, height, bpp, false,
                &tmpDivs[0], &tmpDivs[1],
                &errorMsg, &patch->numYDivs, true) != 0) {
        printf("%s\n", errorMsg);
        return NULL;
    }
    if (patch->numYDivs) {
        patch->yDivs = (int32_t*)malloc(patch->numYDivs*sizeof(int32_t));
        memcpy(patch->yDivs, tmpDivs, patch->numYDivs*sizeof(int32_t));
    }

    // check for degenerate divs
    {
        int i;
        int zeros = 0;
        for (i = 0; i < patch->numYDivs && patch->yDivs[i] == 0; i++) {
            zeros += 1;
        }
        patch->numYDivs -= zeros;
        patch->yDivs += zeros;
        for (i=patch->numYDivs-1; i>=0 && patch->yDivs[i]==height-2; --i) {
            patch->numYDivs -= 1;
        }
    }
    {
        int i;
        int zeros = 0;
        for (i = 0; i < patch->numXDivs && patch->xDivs[i] == 0; i++) {
            zeros += 1;
        }
        patch->numXDivs -= zeros;
        patch->xDivs += zeros;
        for (i=patch->numXDivs-1; i>=0 && patch->xDivs[i]==width-2; --i) {
            patch->numXDivs -= 1;
        }
    }
    return patch;
}
