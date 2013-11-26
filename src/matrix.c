#include "primitives.h"

void
matrix_identity(struct matrix* dest) {
#define M(COLUMN, ROW) dest->m[COLUMN+ROW*4]
    M(0,0) = 1.f; M(0,1) = 0.f; M(0,2) = 0.f; M(0,3) = 0.f;
    M(1,0) = 0.f; M(1,1) = 1.f; M(1,2) = 0.f; M(1,3) = 0.f;
    M(2,0) = 0.f; M(2,1) = 0.f; M(2,2) = 1.f; M(2,3) = 0.f;
    M(3,0) = 0.f; M(3,1) = 0.f; M(3,2) = 0.f; M(3,3) = 1.f;
#undef M
}

void
matrix_multiply_vector(float dest[4], struct matrix const* matrix,
        float const vector[4]) {
    float x=vector[0], y=vector[1], z=vector[2], w=vector[3];
    float const* m = matrix->m;

#define M(COLUMN, ROW) m[COLUMN*4+ROW]
    dest[0] = M(0,0) * x + M(1,0) * y + M(2,0) * z + M(3,0) * w;
    dest[1] = M(0,1) * x + M(1,1) * y + M(2,1) * z + M(3,1) * w;
    dest[2] = M(0,2) * x + M(1,2) * y + M(2,2) * z + M(3,2) * w;
    dest[3] = M(0,3) * x + M(1,3) * y + M(2,3) * z + M(3,3) * w;
#undef M
}

void
matrix_translate_inplace(struct matrix* m, float x, float y, float z) {
#define M(COLUMN, ROW) m->m[COLUMN*4+ROW]
    for (int i=0 ; i<4 ; i++) {
        M(3,i) += M(0,i)*x + M(1,i)*y + M(2,i)*z;
    }
#undef M
}

void
matrix_scale_inplace(struct matrix* m, float x, float y, float z) {
#define M(COLUMN, ROW) m->m[COLUMN*4+ROW]
#define ROW(ROW, V) M(0,ROW)*=V; M(1,ROW)*=V; M(2,ROW)*=V; M(2,ROW)*=V
    ROW(0, x);
    ROW(1, y);
    ROW(2, z);
#undef ROW
#undef M
}

void
matrix_multiply_matrix(struct matrix* restrict dest,
        struct matrix const* restrict a,
        struct matrix const* restrict b) {

#define A(COLUMN, ROW) a->m[COLUMN*4+ROW]
#define B(COLUMN, ROW) b->m[COLUMN*4+ROW]
#define D(COLUMN, ROW) dest->m[COLUMN*4+ROW]
#define CELL(x,y) D(x,y) = A(0,y)*B(x,0) + A(1,y)*B(x,1) + A(2,y)*B(x,2) \
    + A(3,y)*B(x,3)
#define ROW(y) CELL(0,y); CELL(1,y); CELL(2,y); CELL(3,y)
    ROW(0);
    ROW(1);
    ROW(2);
    ROW(3);
#undef CELL
#undef ROW
#undef A
#undef B
#undef D
}

