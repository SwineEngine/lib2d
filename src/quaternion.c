#include "primitives.h"
#include <math.h>

#define MIX_FIELD(FIELD) dest->FIELD = x->FIELD * (1-a) + y->FIELD * a

void
quaternion_copy(struct quaternion* dest,
        struct quaternion const* source) {
    dest->w = source->w;
    dest->x = source->x;
    dest->y = source->y;
    dest->z = source->z;
}

void
quaternion_angle_axis(struct quaternion* dest,
        float angleDeg, float x, float y, float z) {
    float half = angleDeg * (3.1415926f / 360.f);
    float sin = sinf(half) ;
    dest->w = cosf(half);
    dest->x = x * sin;
    dest->y = y * sin;
    dest->z = z * sin;
    quaternion_normalize(dest);
}

void
quaternion_multiply(struct quaternion* dest,
        struct quaternion const* a,
        struct quaternion const* b) {
    struct quaternion res;
    res.w = a->w*b->w - a->x*b->x  -  a->y*b->y  -  a->z*b->z;
    res.x = a->w*b->x + a->x*b->w  +  a->y*b->z  -  a->z*b->y;
    res.y = a->w*b->y + a->y*b->w  +  a->z*b->x  -  a->x*b->z;
    res.z = a->w*b->z + a->z*b->w  +  a->x*b->y  -  a->y*b->x;
    *dest = res;
}

void
quaternion_multiply_vector(float dest[3], struct quaternion const* q,
        float const vector[3]) {
    float v4[4] = {vector[0], vector[1], vector[2], 0.f};
    struct matrix m;
    quaternion_to_matrix(&m, q);
    matrix_multiply_vector(v4, &m, v4);
    dest[0] = v4[0];
    dest[1] = v4[1];
    dest[2] = v4[2];
}

void
quaternion_normalize(struct quaternion* q) {
    float lengthSquared = q->x*q->x + q->y*q->y + q->z*q->z + q->w*q->w;
    float scale = 1.f/sqrtf(lengthSquared);
    q->x *= scale;
    q->y *= scale;
    q->z *= scale;
    q->w *= scale;
}

void
quaternion_nlerp(struct quaternion* dest,
        struct quaternion const* x,
        struct quaternion const* y, float a) {

    MIX_FIELD(w);
    MIX_FIELD(x);
    MIX_FIELD(y);
    MIX_FIELD(z);

    quaternion_normalize(dest);
}

bool
quaternion_is_identity(struct quaternion const* q) {
    return q->w == 1.f && q->x == 0.f && q->y == 0.f && q->z == 0.f;
}

void
quaternion_to_matrix(struct matrix* dest,
        struct quaternion const* source) {
    float* m = dest->m;

    float w = source->w;
    float x = source->x;
    float y = source->y;
    float z = source->z;

#define M(COLUMN, ROW) m[COLUMN*4+ROW]
    M(0,0) = 1 - 2 * y * y - 2 * z * z;
    M(0,1) = 2 * x * y + 2 * w * z;
    M(0,2) = 2 * x * z - 2 * w * y;

    M(1,0) = 2 * x * y - 2 * w * z;
    M(1,1) = 1 - 2 * x * x - 2 * z * z;
    M(1,2) = 2 * y * z + 2 * w * x;

    M(2,0) = 2 * x * z + 2 * w * y;
    M(2,1) = 2 * y * z - 2 * w * x;
    M(2,2) = 1 - 2 * x * x - 2 * y * y;

    M(0,3) = 0;
    M(1,3) = 0;
    M(2,3) = 0;
    M(3,0) = 0;
    M(3,1) = 0;
    M(3,2) = 0;
    M(3,3) = 1;
#undef M
}
