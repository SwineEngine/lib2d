#ifndef __primitives_H__
#define __primitives_H__

#include <stdbool.h>

struct quaternion {
    float w, x, y, z;
};
struct rect {
    float l, t, r, b;
};
struct site {
    struct rect rect;
    struct quaternion quaternion;
    float x, y, z;
    float scale;
    float wrap[4];
};

struct matrix {
    float m[16];
};


void
matrix_identity(struct matrix* dest);

void
matrix_multiply_vector(float dest[4], struct matrix const* matrix,
        float const vector[4]);

void
matrix_translate_inplace(struct matrix* m, float x, float y, float z);

void
matrix_scale_inplace(struct matrix* m, float x, float y, float z);

void
matrix_multiply_matrix(struct matrix* restrict dest,
        struct matrix const* restrict a,
        struct matrix const* restrict b);



void
quaternion_normalize(struct quaternion* q);

void
quaternion_angle_axis(struct quaternion* dest,
        float angleDeg, float x, float y, float z);

void
quaternion_multiply(struct quaternion* dest,
        struct quaternion const* a,
        struct quaternion const* b);

void
quaternion_to_matrix(struct matrix* dest,
        struct quaternion const* source);

void
quaternion_nlerp(struct quaternion* dest,
        struct quaternion const* x,
        struct quaternion const* y, float a);

bool
quaternion_is_identity(struct quaternion const* q);

void
quaternion_multiply_vector(float dest[3], struct quaternion const* q,
        float const vector[3]);

void
quaternion_copy(struct quaternion* dest,
        struct quaternion const* source);


void
site_copy(struct site* dest, struct site const* source);

void
site_init(struct site* site);

bool
site_intersect_point(struct site const* site,
        float x, float y, float* outRelative);

float
site_wrap(float v, const float* bounds);

#endif
