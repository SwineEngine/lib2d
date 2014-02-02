#include "primitives.h"
#include "math.h"

static
void
rect_copy(struct rect* dest, struct rect const* source) {
    dest->l = source->l;
    dest->t = source->t;
    dest->r = source->r;
    dest->b = source->b;
}

float
site_wrap(float v, const float* bounds) {
    float l = bounds[0];
    float h = bounds[1];
    if (!l && !h) return v;
    float d = h-l;
    v = fmodf(v - fmodf(l,d), d);
    if (v < 0)
        v += d;
    return v + l;
}

void
site_copy(struct site* dest, struct site const* source) {
    rect_copy(&dest->rect, &source->rect);
    quaternion_copy(&dest->quaternion, &source->quaternion);
    dest->x = source->x;
    dest->y = source->y;
    dest->z = source->z;
    dest->scale = source->scale;
    dest->wrap[0] = source->wrap[0];
    dest->wrap[1] = source->wrap[1];
    dest->wrap[2] = source->wrap[2];
    dest->wrap[3] = source->wrap[3];
}

void
site_apply_parent(struct site* dest, struct site const* parent) {
    quaternion_multiply(&dest->quaternion, &dest->quaternion, &parent->quaternion);
    float vec[3];
    vec[0] = dest->x * parent->scale;
    vec[1] = dest->y * parent->scale;
    vec[2] = dest->z * parent->scale;
    if (parent->quaternion.w != 1.f)
        quaternion_multiply_vector(vec, &parent->quaternion, vec);
    dest->x = vec[0]+site_wrap(parent->x, parent->wrap);
    dest->y = vec[1]+site_wrap(parent->y, parent->wrap+2);
    dest->z = vec[2]+parent->z;
    dest->scale *= parent->scale;
}

void
site_init(struct site* site) {
    site->rect.l = 0.f;
    site->rect.t = 0.f;
    site->rect.r = 1.f;
    site->rect.b = 1.f;
    site->quaternion.w = 1.f;
    site->quaternion.x = 0.f;
    site->quaternion.y = 0.f;
    site->quaternion.z = 0.f;
    site->x = 0.f;
    site->y = 0.f;
    site->z = 0.f;
    site->scale = 1.f;
    site->wrap[0] = 0;
    site->wrap[1] = 0;
    site->wrap[2] = 0;
    site->wrap[3] = 0;
}

bool
site_intersect_point(struct site const* site,
        float x, float y, float* outRelative) {
    // make relative to center point:
    x -= site_wrap(site->x, site->wrap);
    y -= site_wrap(site->y, site->wrap+2);
    // make relative to scale:
    x /= site->scale;
    y /= site->scale;

    // TODO account for z

    // TODO account for quaternion

    if (outRelative) {
        outRelative[0] = x - site->rect.l;
        outRelative[1] = y - site->rect.t;
        outRelative[2] = site->rect.r - site->rect.l;
        outRelative[3] = site->rect.b - site->rect.t;
    }

    struct rect const* r = &site->rect;
    return (x >= r->l) && (x <= r->r) && (y >= r->t) && (y <= r->b);
}


