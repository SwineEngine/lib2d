#include "lib2d.h"
#include "stretchy_buffer.h"
#include <math.h>

const static uint32_t IS_REVERSED = 1<<31;
const static uint32_t START_V_SET = 1<<30;
#ifndef M_PI
const double M_PI = 3.1415926535897931;
#endif
#define mix(a, b, r)((a)*(1-(r)) + (b)*(r))


struct l2d_anim {
    float to_v, start_v;
    float time_left, time_past;
    uint32_t flags;
    struct l2d_anim* next;
};

struct l2d_anim** anim_pool = NULL;

static
struct l2d_anim*
grab_anim() {
    if (sbcount(anim_pool)) {
        struct l2d_anim* a = sblast(anim_pool);
        sbremove(anim_pool, sbcount(anim_pool)-1, 1);
        return a;
    } else {
        struct l2d_anim** a = sbadd(anim_pool, 63);
        struct l2d_anim* d = malloc(sizeof(struct l2d_anim)*64);
        for (int i=0; i<63; i++) {
            a[i] = d+i;
        }
        return d+63;
    }
}

static
void
release_anim(struct l2d_anim* a) {
    sbpush(anim_pool, a);
}

bool
l2d_anim_step(struct l2d_anim** first, float dt, float* dst) {
    if (*first == NULL) return false;
    struct l2d_anim* a = *first;
    if (!(a->flags&START_V_SET)) {
        a->start_v = *dst;
        a->flags |= START_V_SET;
    }
    float from = a->start_v;
    while (a) {
        a->time_past += dt;
        a->time_left -= dt;
        float r = a->time_left / (a->time_left+a->time_past);
        if (a->time_left > 0 || (a->flags&(l2d_ANIM_REPEAT|l2d_ANIM_REVERSE))) {
            if (a->flags&l2d_ANIM_EASE_IN && a->flags&l2d_ANIM_EASE_OUT) {
                r = -cosf(r * M_PI)*.5 + .5;
            } else if (a->flags&l2d_ANIM_EASE_IN) {
                r = 1 - cosf(r * M_PI*.5);
            } else if (a->flags&l2d_ANIM_EASE_OUT) {
                r = sinf(r * M_PI*.5);
            }
        }
        if ((a->flags&l2d_ANIM_EXTRAPOLATE) && a->next == NULL) {
            *dst = from = mix(a->to_v, from, r);
            a = a->next;
        } else {
            if (r <= 0 && (a->flags&(l2d_ANIM_REPEAT|l2d_ANIM_REVERSE)) && a->next == NULL) {
                r = 1-r;
                a->time_left = a->time_past + a->time_left;
                a->time_past = 0;
                if (a->flags & l2d_ANIM_REVERSE)
                    a->flags ^= IS_REVERSED;
            }
            if (a->flags & IS_REVERSED) {
                r = 1-r;
                if (r <= 0) r = 0.01;
            }
            if (r <= 0) {
                (*first)->start_v = *dst = from = a->to_v;
                struct l2d_anim* stopat = a->next;
                a = a->next;
                for (struct l2d_anim* to_rem = *first; to_rem != stopat; to_rem = *first) {
                    if (to_rem->next) {
                        to_rem->next->start_v = to_rem->start_v;
                        to_rem->next->flags |= START_V_SET;
                    }
                    *first = to_rem->next;
                    release_anim(to_rem);
                }
            } else {
                *dst = from = mix(a->to_v, from, r);
                a = a->next;
            }
        }
    }
    return true;
}

void
l2d_anim_new(struct l2d_anim** anim_list, float to_v, float dt, uint32_t flags) {
    struct l2d_anim* a = grab_anim();
    a->to_v = to_v;
    a->start_v = 0;
    a->time_left = dt;
    a->time_past = 0;
    a->next = NULL;
    a->flags = flags;
    if (*anim_list == NULL) {
        *anim_list = a;
    } else {
        struct l2d_anim* n = *anim_list;
        while (n) {
            if (n->next == NULL) {
                n->next = a;
                break;
            }
            n = n->next;
        }
    }
}

void
l2d_anim_release_all(struct l2d_anim** anims) {
    struct l2d_anim* a = *anims;
    while (a) {
        release_anim(a);
        a = a->next;
    }
    *anims = NULL;
}
