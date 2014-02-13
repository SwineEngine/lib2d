#include "lib2d.h"
#include "renderer.h"
#include "image_bank.h"
#include "nine_patch.h"
#include "stretchy_buffer.h"
#include "render_api.h"
#include "effect.h"
#include "target.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// TODO don't hard code something here.
#define MAX_VERTICIES 1024

struct drawer_attribute {
    l2d_ident name;
    int size; // number of floats per vertex
    float* data; // stretchy buffer
};
static
float
min3(float a, float b, float c) {
    if (a < b && a < c) {
        return a;
    } else if (b < a && b < c) {
        return b;
    } else {
        return c;
    }
}

static
float
max3(float a, float b, float c) {
    if (a > b && a > c) {
        return a;
    } else if (b > a && b > c) {
        return b;
    } else {
        return c;
    }
}

static
bool
rect_intersect(struct rect const* a, struct rect const* b) {
    return a->r >= b->l && a->l <= b->r && a->b >= b->t && a->t <= b->b;
}

static
void
premult_site_to_matrix(struct matrix* m, struct site const* site) {
    matrix_translate_inplace(m,
            site_wrap(site->x, site->wrap),
            site_wrap(site->y, site->wrap+2),
            site->z);
    if (site->quaternion.w != 1.f) {
        struct matrix quat_m;
        quaternion_to_matrix(&quat_m, &site->quaternion);
        struct matrix m2;
        matrix_multiply_matrix(&m2, m, &quat_m);
        *m = m2;
    }
    matrix_scale_inplace(m, site->scale, site->scale, site->scale);
    matrix_translate_inplace(m, site->rect.l, site->rect.t, 0.f);
}

struct drawer {
    struct ir* ir;
    struct drawer* next;
    struct drawer** prev;
    struct site site;
    struct l2d_image* image;
    struct l2d_effect* effect;
    float alpha;
    float desaturate;
    float color[4];
    struct material* material;
    struct l2d_target* target;
    int order;
    enum l2d_blend blend;

    struct geo_vert* geoVerticies; // stretchy buffer
    unsigned short* geoIndicies;

    struct drawer_attribute* attributes; // stretchy buffer

    struct drawer_mask* mask;

    struct site clip_site;
    bool clip_site_set;
};

struct drawer_mask {
    struct ir* ir;
    struct drawer_mask* next;
    struct drawer_mask** prev;
    struct site site;
    struct l2d_image* image;
    float alpha;
};

struct mat_cache_entry {
    int effect_id;
    enum l2d_blend blend;
    struct material* material;
};

static
void
drawer_update_material(struct drawer* d) {
    if (d->effect == NULL) {
        if (ib_image_format(d->image) == l2d_IMAGE_FORMAT_A_8) {
            d->material = d->ir->singleChannelDefaultMaterial;
        } else if (d->blend == l2d_BLEND_PREMULT) {
            d->material = d->ir->premultMaterial;
        } else {
            d->material = d->ir->defaultMaterial;
        }
    } else {
        sbforeachp(struct mat_cache_entry* e, d->ir->material_cache) {
            if (e->effect_id == d->effect->id && e->blend == d->blend) {
                d->material = e->material;
                return;
            }
        }

        enum shader_type t = SHADER_DEFAULT;
        if (ib_image_format(d->image) == l2d_IMAGE_FORMAT_A_8) {
            t = SHADER_SINGLE_CHANNEL;
        } else if (d->blend == l2d_BLEND_PREMULT) {
            t = SHADER_PREMULT;
        }

        // TODO cache material
        d->material = render_api_material_new(
            render_api_load_shader(t), d->effect);

        struct mat_cache_entry* e = sbadd(d->ir->material_cache, 1);
        e->effect_id = d->effect->id;
        e->blend = d->blend;
        e->material = d->material;
    }
}

struct ir*
ir_new(struct l2d_image_bank* ib) {
    struct ir* ir = (struct ir*)malloc(sizeof(struct ir));

    ir->ib = ib;
    ir->targetList = NULL;
    ir->drawerList = NULL;
    init_sort_cache(&ir->sort_cache);
    ir->viewportWidth = 1;
    ir->viewportHeight = 1;
    ir->translate[0] = 0;
    ir->translate[1] = 0;
    ir->translate[2] = 0;

    ir->defaultMaterial = render_api_material_new(
            render_api_load_shader(SHADER_DEFAULT), NULL);
    ir->premultMaterial = render_api_material_new(
            render_api_load_shader(SHADER_PREMULT), NULL);
    ir->singleChannelDefaultMaterial = render_api_material_new(
            render_api_load_shader(SHADER_SINGLE_CHANNEL), NULL);
    ir->material_cache = NULL;

    ir->scratchVerticies = NULL;
    ir->scratchIndicies = NULL;
    ir->scratchAttributes = NULL;

    ir->maskList = NULL;

    int res[4];
    render_api_get_viewport(res);
    ir->viewportWidth = res[2];
    ir->viewportHeight = res[3];

    return ir;
}

void
init_sort_cache(struct sort_cache* c){ 
    c->sort_buffer_dirty = false;
    c->sort_order_dirty = false;
    c->buffer = NULL;
    c->alloc_size = 0;
    c->drawer_count = 0;
}

void
ir_delete(struct ir* ir) {
    while (ir->drawerList != NULL) {
        drawer_delete(ir->drawerList);
    }

    if (ir->sort_cache.buffer)
        free(ir->sort_cache.buffer);

    // TODO delete all created shaders.
    // TODO delete all cached materials.

    sbfree(ir->scratchVerticies);
    sbfree(ir->scratchIndicies);
    // TODO clean up scratchAttributes

    free(ir);
}

struct drawer*
drawer_new(struct ir* ir) {
    ir->sort_cache.sort_buffer_dirty = true;
    struct drawer* drawer =
        (struct drawer*)malloc(sizeof(struct drawer));
    drawer->ir = ir;
    drawer->next = ir->drawerList;
    if (ir->drawerList) {
        ir->drawerList->prev = &drawer->next;
    }
    ir->drawerList = drawer;
    drawer->prev = & ir->drawerList;

    drawer->image = NULL;
    drawer->effect = NULL;

    site_init(&drawer->site);
    drawer->site.x = -2.f;

    drawer->alpha = 1.f;
    drawer->desaturate = 0.f;
    drawer->color[0] = 1;
    drawer->color[1] = 1;
    drawer->color[2] = 1;
    drawer->color[3] = 1;

    drawer->material = ir->defaultMaterial;
    drawer->target = NULL;

    drawer->order = 0;

    drawer->blend = l2d_BLEND_DEFAULT;

    drawer->geoVerticies = NULL;
    drawer->geoIndicies = NULL;
    drawer->attributes = NULL;

    drawer->mask = NULL;

    drawer->clip_site_set = false;

    return drawer;
}

void
drawer_delete(struct drawer* drawer) {
    drawer->ir->sort_cache.sort_buffer_dirty = true;
    *drawer->prev = drawer->next;
    if (drawer->next) {
        drawer->next->prev = drawer->prev;
    }
    if (drawer->image) {
        ib_image_decref(drawer->image);
        free(drawer);
    }
    // TODO cleanup vertex data.
}

struct geo_vert {
    float x,y,u,v;
};

void
drawer_copy(struct drawer* dst, struct drawer const* src) {
    dst->ir->sort_cache.sort_order_dirty = true;
    site_copy(&dst->site, &src->site);
    dst->image = src->image;
    if (dst->image)
        ib_image_incref(dst->image);
    dst->alpha = src->alpha;
    dst->desaturate = src->desaturate;
    dst->material = src->material;
    dst->target = src->target;
    dst->order = src->order;
    dst->mask = src->mask;

    drawer_clear_geo(dst);

    if (sbcount(src->geoVerticies)) {
        sbconcat(dst->geoVerticies, src->geoVerticies);
    }
    if (sbcount(src->geoIndicies)) {
        sbconcat(dst->geoIndicies, src->geoIndicies);
    }
    for (int i=0; i<sbcount(src->attributes); i++) {
        struct drawer_attribute* a = &src->attributes[i];
        if (sbcount(a->data)) {
            drawer_add_geo_attribute(dst, a->name, a->size, a->data,
                    sbcount(a->data)/a->size);
        }
    }
}

void
drawer_set_effect(struct drawer* d, struct l2d_effect* e) {
    d->ir->sort_cache.sort_order_dirty = true;
    d->effect = e;
    drawer_update_material(d);
}

void
drawer_add_geo_rect(struct drawer* d,
        struct rect pos, struct rect tex) {
    int start = sbcount(d->geoVerticies);
    if (start+4 > MAX_VERTICIES) {
        assert(false);
        return;
    }
    struct geo_vert* v = sbadd(d->geoVerticies, 4);
#define CORNER(X, Y) \
    v->x = pos.X; v->y = pos.Y; v->u = tex.X; v->v = tex.Y; v++
    CORNER(l, t);
    CORNER(r, t);
    CORNER(r, b);
    CORNER(l, b);
#undef CORNER
    unsigned short* ind = sbadd(d->geoIndicies, 6);
    int i = 0;
    ind[i++] = start+0;
    ind[i++] = start+1;
    ind[i++] = start+2;
    ind[i++] = start+0;
    ind[i++] = start+2;
    ind[i++] = start+3;
}

void
drawer_add_geo_2d(struct drawer* d,
        struct vert_2d* verticies, unsigned int vert_count,
        unsigned int* indicies, unsigned int index_count) {
    int start = sbcount(d->geoVerticies);
    struct geo_vert* v = sbadd(d->geoVerticies, vert_count);
    for (int i=0; i<vert_count; i++) {
        v[i].x = verticies[i].x;
        v[i].y = verticies[i].y;
        v[i].u = verticies[i].u;
        v[i].v = verticies[i].v;
    }
    unsigned short* ind = sbadd(d->geoIndicies, index_count);
    for (int i=0; i<index_count; i++) {
        ind[i] = (unsigned short)(start + indicies[i]);
    }
}

void
drawer_add_geo_attribute(struct drawer* d,
        l2d_ident attribute,
        unsigned int size, float* verticies, unsigned int vert_count) {
    struct drawer_attribute* a=NULL;
    // first, find an existing attribute with that name:
    for (int i=0; i<sbcount(d->attributes); i++) {
        if (d->attributes[i].name == attribute) {
            a = &d->attributes[i];
            assert(a->size == size);
            break;
        }
    }
    if (!a) {
        a = sbadd(d->attributes, 1);
        a->name = attribute;
        a->size = size;
        a->data = NULL;
    }

    int float_count = size*vert_count;
    memcpy(sbadd(a->data, float_count), verticies,
            float_count*sizeof(float));
}

void
drawer_clear_geo(struct drawer* d) {
    if (d->geoVerticies)
        sbremove(d->geoVerticies, 0, sbcount(d->geoVerticies));
    if (d->geoIndicies)
        sbremove(d->geoIndicies, 0, sbcount(d->geoIndicies));
    for (int i=0; i<sbcount(d->attributes); i++) {
        sbempty(d->attributes[i].data);
    }
}

void
drawer_set_site(struct drawer* drawer, struct site const* site) {
    site_copy(&drawer->site, site);
}

void
drawer_set_image(struct drawer* drawer, struct l2d_image* image) {
    if (image == drawer->image) return;
    drawer->ir->sort_cache.sort_order_dirty = true;
    if (drawer->image)
        ib_image_decref(drawer->image);
    drawer->image = image;
    if (drawer->image)
        ib_image_incref(drawer->image);
    drawer_update_material(drawer);
}

void
drawer_set_desaturate(struct drawer* drawer, float desaturate) {
    drawer->desaturate = desaturate;
}

void
drawer_set_color(struct drawer* drawer, float color[4]) {
    drawer->color[0] = color[0];
    drawer->color[1] = color[1];
    drawer->color[2] = color[2];
    drawer->color[3] = color[3];
}


void
drawer_setMaterial(struct drawer* drawer,
        struct material* material) {
    drawer->ir->sort_cache.sort_order_dirty = true;
    if (material == NULL) {
        material = drawer->ir->defaultMaterial;
    }
    drawer->material = material;
}

void
drawer_set_target(struct drawer* drawer, struct l2d_target* target) {
    if (target == drawer->target) return;
    drawer->target = target;

    drawer->ir->sort_cache.sort_buffer_dirty = true;
    if (target)
        target->sort_cache.sort_buffer_dirty = true;

    // unlink from old list:
    if (drawer->next) {
        drawer->next->prev = drawer->prev;
    }
    *drawer->prev = drawer->next;

    struct drawer** drawerListP =
        target ? &target->drawerList : &drawer->ir->drawerList;

    // add to new target:
    drawer->next = *drawerListP;
    if (drawer->next) {
        drawer->next->prev = &drawer->next;
    }
    drawer->prev = drawerListP;
    *drawerListP = drawer;
}

void
drawer_setOrder(struct drawer* drawer, int order) {
    drawer->ir->sort_cache.sort_order_dirty = true;
    drawer->order = order;
}

void
drawer_set_clip_site(struct drawer* drawer,
        struct site const* site) {
    drawer->ir->sort_cache.sort_order_dirty = true;
    if (site) {
        drawer->clip_site_set = true;
        site_copy(&drawer->clip_site, site);
    } else {
        drawer->clip_site_set = false;
    }
}

void
drawer_blend(struct drawer* drawer, enum l2d_blend blend) {
    drawer->ir->sort_cache.sort_order_dirty = true;
    drawer->blend = blend;
    drawer_update_material(drawer);
}

void
drawer_set_mask(struct drawer* drawer, struct drawer_mask* mask) {
    drawer->ir->sort_cache.sort_order_dirty = true;
    drawer->mask = mask;
}

struct drawer_mask*
drawer_mask_new(struct ir* ir) {
    struct drawer_mask* mask = malloc(sizeof(struct drawer_mask));
    mask->ir = ir;

    mask->next = ir->maskList;
    if (ir->maskList) {
        ir->maskList->prev = &mask->next;
    }
    ir->maskList = mask;
    mask->prev = &ir->maskList;

    site_init(&mask->site);
    mask->alpha = 1.f;

    mask->image = NULL;

    return mask;
}

struct drawer_mask*
drawer_mask_clone(struct ir* ir, struct drawer_mask* old) {
    struct drawer_mask* m = drawer_mask_new(ir);
    drawer_mask_set_image(m, old->image);
    drawer_mask_set_site(m, &old->site);
    drawer_mask_set_alpha(m, old->alpha);
    return m;
}

void
drawer_mask_set_image(struct drawer_mask* mask,
        struct l2d_image* image) {
    if (image == mask->image) return;
    if (mask->image)
        ib_image_decref(mask->image);
    mask->image = image;
    if (mask->image)
        ib_image_incref(mask->image);
}

void
drawer_mask_set_site(struct drawer_mask* mask,
        struct site const* site) {
    site_copy(&mask->site, site);
}

void
drawer_mask_set_alpha(struct drawer_mask* mask, float a) {
    mask->alpha = a;
}

struct data_output {
    struct vertex* verticies;
    int posIndex;
    int texIndex;
    unsigned short* indicies;
    int indexIndex;
    int indexStart;
    struct site* site;
    struct matrix matrix;
    float alpha;
    float desaturate;
    struct rect texture_region;
    bool clip;
    struct rect outer_clip;
    float color[4];
};

static
void
transform(struct matrix* m, float x, float y, float* out) {
    float vec[4] = {x,y,0.f,1.f};
    matrix_multiply_vector(vec, m, vec);
    x = vec[0]; y = vec[1];
    float w = vec[3];

    out[0] = x/w;
    out[1] = y/w;
}

static
void
pos(struct data_output* d, float x, float y) {
    struct vertex* v = &d->verticies[d->posIndex];

    transform(&d->matrix, x, y, v->position);

    v->position[2] = 0.f;
    v->position[3] = 1.f;

    v->misc[0] = d->alpha;
    v->misc[1] = d->desaturate;

    v->color[0] = d->color[0];
    v->color[1] = d->color[1];
    v->color[2] = d->color[2];
    v->color[3] = d->color[3];

    d->posIndex ++;
}

static
void
tex(struct data_output* d, float x, float y) {
    struct vertex* v = &d->verticies[d->texIndex];
    v->texCoord[0] = (1-x) * d->texture_region.l + x * d->texture_region.r;
    v->texCoord[1] = (1-y) * d->texture_region.t + y * d->texture_region.b;
    d->texIndex ++;
}

static
void
face(struct data_output* d, unsigned int a, unsigned int b, unsigned int c) {
    if (d->clip) {
        float* va = d->verticies[a].position;
        float* vb = d->verticies[b].position;
        float* vc = d->verticies[c].position;
        struct rect bounds = {
            .l=min3(va[0], vb[0], vc[0]),
            .t=min3(va[1], vb[1], vc[1]),
            .r=max3(va[0], vb[0], vc[0]),
            .b=max3(va[1], vb[1], vc[1])
        };
        if (!rect_intersect(&bounds, &d->outer_clip)) {
            return;
        }
    }
    d->indicies[d->indexIndex++] = (unsigned short)(d->indexStart + a);
    d->indicies[d->indexIndex++] = (unsigned short)(d->indexStart + b);
    d->indicies[d->indexIndex++] = (unsigned short)(d->indexStart + c);
}

static
void
generateRect(struct data_output* d, int width, int height) {
    pos(d, 0.f, 0.f);
    pos(d, width, 0.f);
    pos(d, width, height);
    pos(d, 0.f, height);
    tex(d, 0.f, 0.f);
    tex(d, 1.f, 0.f);
    tex(d, 1.f, 1.f);
    tex(d, 0.f, 1.f);
    face(d, 0,1,2);
    face(d, 0,2,3);
}

static
int
drawerSortCompare(const void* aVoidP, const void* bVoidP) {
    const struct drawer* a = *(const struct drawer**)aVoidP;
    const struct drawer* b = *(const struct drawer**)bVoidP;

    if (a->order != b->order) {
        return a->order - b->order;
    }

    int r = image_sort_compare(a->image, b->image);
    if (r) {
        return r;
    }

    if (a->material != b->material) {
        return (size_t)a->material - (size_t)b->material;
    }

    if (a->mask != b->mask) {
        return a->mask - b->mask;
    }

    if (a->blend != b->blend) {
        return (int)a->blend - (int)b->blend;
    }

    return a - b;
}

static
void
batch_reset(struct batch* b, struct material* m) {
    int num_attrs;
    struct material_attribute* attrs = render_api_get_attributes(m, &num_attrs);
    int needed = num_attrs;
    int have = sbcount(b->attributes);
    if (have < needed) {
        for (int i=0; i<needed-have; i++) {
            struct attribute* a = sbadd(b->attributes, 1);
            a->name = 0;
            a->data = NULL;
        }
    }

    // zero all
    for (int i=0; i<sbcount(b->attributes); i++) {
        struct attribute* a = &b->attributes[i];
        a->name = 0;
        a->size = 0;
        sbempty(a->data);
    }

    // fill in:
    for (int i=0; i<num_attrs; i++) {
        struct attribute* a = &b->attributes[i];
        a->name = attrs[i].name;
        a->size = attrs[i].size;
    }
}

static
void
batch_add(struct batch* batch, struct drawer* d, int viewportWidth,
        int viewportHeight, struct matrix const* projection_matrix) {
    struct site* site = &d->site;
    float width = site->rect.r - site->rect.l;
    float height = site->rect.b - site->rect.t;
    float alpha = d->alpha;
    float desaturate = d->desaturate;

    int vertexCount;
    int indexCount;

    struct nine_patch* nine_patch = image_get_nine_patch(d->image);
    if (nine_patch) {
        drawer_clear_geo(d);
        struct build_params params = {.image=d->image, .geoVerticies=d->geoVerticies,
            .geoIndicies=d->geoIndicies, .bounds_width=width, .bounds_height=height};
        // TODO cache built nine patch
        nine_patch_build_geo(&params);
        d->geoVerticies = params.geoVerticies;
        d->geoIndicies = params.geoIndicies;
    }

    if (sbcount(d->geoVerticies) == 0) {
        vertexCount = 4;
        indexCount = 6;
    } else {
        vertexCount = sbcount(d->geoVerticies);
        indexCount = sbcount(d->geoIndicies);
    }

    if (batch->vertexCount + vertexCount > sbcount(batch->verticies)) {
        // Empty if statement to suppress warning of unused value
        if (sbadd(batch->verticies, vertexCount)) {}
    }
    if (batch->indexCount + indexCount > sbcount(batch->indicies)) {
        if (sbadd(batch->indicies, indexCount)) {}
    }

    struct data_output data_output = {
        .verticies = batch->verticies+batch->vertexCount,
        .indicies = batch->indicies+batch->indexCount,
        .indexStart = batch->vertexCount,
        .site = site,
        .alpha = alpha,
        .desaturate = desaturate,
        .texture_region = ib_image_get_texture_region(d->image),
        .matrix = *projection_matrix,
        .clip = false,
    };
    data_output.color[0] = d->color[0];
    data_output.color[1] = d->color[1];
    data_output.color[2] = d->color[2];
    data_output.color[3] = d->color[3];

    premult_site_to_matrix(&data_output.matrix, site);

    if (d->clip_site_set) {
        data_output.clip = true;
        struct matrix clip_matrix = *projection_matrix;
        premult_site_to_matrix(&clip_matrix, &d->clip_site);
        float width = d->clip_site.rect.r - d->clip_site.rect.l;
        float height = d->clip_site.rect.b - d->clip_site.rect.t;
        float verts[] = {0.f,0.f, width,0.f, width,height, 0.f,height};
        for (int i=0;i<8;i+=2) {
            float out[2];
            transform(&clip_matrix, verts[i+0], verts[i+1], &out[0]);
            if (i == 0) {
                data_output.outer_clip.l = out[0];
                data_output.outer_clip.t=out[1];
                data_output.outer_clip.r=out[0];
                data_output.outer_clip.b=out[1];
            } else {
                if (data_output.outer_clip.l > out[0])
                    data_output.outer_clip.l = out[0];
                if (data_output.outer_clip.r < out[0])
                    data_output.outer_clip.r = out[0];
                if (data_output.outer_clip.t > out[1])
                    data_output.outer_clip.t = out[1];
                if (data_output.outer_clip.b < out[1])
                    data_output.outer_clip.b = out[1];
            }
        }
    }

    if (sbcount(d->geoVerticies) == 0) {
        generateRect(&data_output, width, height);
    } else {
        for (int i=0; i<sbcount(d->geoVerticies); i++) {
            struct geo_vert* v = &d->geoVerticies[i];
            if (nine_patch) {
                pos(&data_output, v->x, v->y);
            } else {
                pos(&data_output, v->x*width, v->y*height);
            }
            tex(&data_output, v->u, v->v);
        }
        unsigned short* ind = d->geoIndicies;
        for (int i=0; i<sbcount(d->geoIndicies); i+=3) {
            face(&data_output, ind[i+0], ind[i+1], ind[i+2]);
        }

        // for each attribute in material
        for (int i=0; i<sbcount(batch->attributes); i++) {
            struct attribute* a = &batch->attributes[i];
            if (a->name == 0) break;
            // find attribute in drawer
            struct drawer_attribute* da = NULL;
            for (int j=0; j<sbcount(d->attributes); j++) {
                if (d->attributes[i].name == a->name) {
                    da = &d->attributes[i];
                    if (!sbcount(da->data)) {
                        da = NULL; // treat it as missing if it is empty
                    }
                    break;
                }
            }
            int float_count = a->size * sbcount(d->geoVerticies);
            int byte_count = float_count * sizeof(float);
            float* dest = sbadd(a->data, float_count);
            if (da) {
                assert(a->size == da->size);
                // TODO validate that there is enough data for the verticies,
                // instead of potentially reading junk data?
                memcpy(dest, da->data, byte_count);
            } else {
                memset(dest, 0, byte_count);
            }
        }
    }

    assert(data_output.posIndex <= vertexCount);
    assert(data_output.indexIndex <= indexCount);

    batch->vertexCount += data_output.posIndex;
    batch->indexCount += data_output.indexIndex;
}

static
void
batch_flush(struct batch* batch,
        struct material* material,
        struct l2d_image* image, enum l2d_blend blend,
        struct drawer_mask* mask,
        bool desaturate,
        int viewportWidth, int viewportHeight) {
    if (batch->indexCount == 0) {
        //assert(batch->vertexCount == 0);
        return;
    }

    unsigned int shader_variant = 0;
    if (mask) {
        shader_variant |= SHADER_MASK;
    }
    if (desaturate) {
        shader_variant |= SHADER_DESATURATE;
    }

    struct shader_handles* shader;
    struct material_handles* h;
    int texture_slot=0;

    render_api_material_use(material, shader_variant, &shader, &h, &texture_slot);

    ib_image_bind(image, shader->texturePixelSizeHandle, shader->textureHandle, texture_slot);
    texture_slot++;

    if (mask) {
        if (shader->maskTexture != -1) {
            ib_image_bind(mask->image, -1, shader->maskTexture, texture_slot);
            texture_slot++;

            struct site* site = &mask->site;

            struct matrix m1;
            matrix_identity(&m1);

            // scale/translate for texture atlasing:
            struct rect r = ib_image_get_texture_region(mask->image);
            matrix_translate_inplace(&m1, r.l, r.t, 0.f);
            matrix_scale_inplace(&m1, (r.r-r.l), (r.b-r.t), 1.f);

            // scale from pixel sizes to [0..1]
            matrix_scale_inplace(&m1, 1.f/(site->rect.r-site->rect.l),
                    1.f/(site->rect.b-site->rect.t), 1.f);

            matrix_translate_inplace(&m1, -site->rect.l, -site->rect.t,
                    0.f);
            float scale = 1.f/site->scale;
            matrix_scale_inplace(&m1, scale, scale, scale);

            struct matrix m2;
            if (site->quaternion.w != 1.f) {
                struct matrix quat_m;
                quaternion_normalize(&site->quaternion);
                struct quaternion q = {
                    .w = site->quaternion.w,
                    .x = -site->quaternion.x,
                    .y = -site->quaternion.y,
                    .z = -site->quaternion.z};
                quaternion_to_matrix(&quat_m, &q);
                matrix_multiply_matrix(&m2, &m1, &quat_m);
            } else {
                m2 = m1;
            }

            struct matrix m3;
            matrix_identity(&m3);
            matrix_translate_inplace(&m3, -site->x, -site->y, -site->z);

            matrix_scale_inplace(&m3,
                    1.f/(2.f/viewportWidth),
                    1.f/(-2.f/viewportHeight),
                    1.f);
            matrix_translate_inplace(&m3, 1.f, -1.f, 0.f);

            struct matrix final;
            matrix_multiply_matrix(&final, &m2, &m3);

            //TODO glUniformMatrix4fv(shader->maskTextureCoordMat, 1, 0, final.m);
        }
    }



    if (shader->eyePos != -1) {
        // Must match projection matrix
        render_api_set_vec(shader->eyePos, 0.f, 0.f, -4.f*viewportHeight, 1.f);
    }

    render_api_draw_batch(batch, shader, material, h, blend);

    batch->indexCount = 0;
    batch->vertexCount = 0;
}

static
void
drawDrawerList(struct batch* batch, struct drawer* drawerList,
        int viewportWidth, int viewportHeight, float* translate,
        struct sort_cache* sort_cache) {
    struct matrix projection_matrix;
    matrix_identity(&projection_matrix);
    projection_matrix.m[2*4+3] = .5f/viewportWidth; // must match eyePos calc
    matrix_translate_inplace(&projection_matrix, -1.f, 1.f, 0.f);
    matrix_scale_inplace(&projection_matrix, 2.f/viewportWidth,
            -2.f/viewportHeight, 1.f);
    if (translate) {
        matrix_translate_inplace(&projection_matrix, translate[0],
                translate[1], translate[2]);
    }

    if (sort_cache->sort_buffer_dirty) {
        sort_cache->sort_buffer_dirty = false;
        sort_cache->sort_order_dirty = true;
        sort_cache->drawer_count = 0;
        for (struct drawer* drawer = drawerList;
                drawer != NULL; drawer = drawer->next) {
            if (sort_cache->drawer_count == sort_cache->alloc_size) {
                sort_cache->alloc_size += 128;
                sort_cache->buffer = realloc(sort_cache->buffer, sort_cache->alloc_size*sizeof(void*));
            }
            sort_cache->buffer[sort_cache->drawer_count++] = drawer;
        }
    }

    if (sort_cache->drawer_count == 0)
        return;

    if (sort_cache->sort_order_dirty) {
        sort_cache->sort_order_dirty = false;
        qsort(sort_cache->buffer, sort_cache->drawer_count, sizeof(struct drawer*),
            drawerSortCompare);
    }

    struct material* material = sort_cache->buffer[0]->material;
    batch_reset(batch, material);
    struct l2d_image* image = sort_cache->buffer[0]->image;
    enum l2d_blend blend = sort_cache->buffer[0]->blend;
    struct drawer_mask* mask = sort_cache->buffer[0]->mask;
    bool desaturate = sort_cache->buffer[0]->desaturate;
    for (int i = 0; i < sort_cache->drawer_count; i++) {
        struct drawer* drawer = sort_cache->buffer[i];
        if (!ib_image_same_texture(drawer->image, image)
                || drawer->material != material
                || drawer->blend != blend
                || drawer->mask != mask
                || (drawer->desaturate!=0) != desaturate) {
            batch_flush(batch, material, image, blend, mask, desaturate,
                    viewportWidth, viewportHeight);
            desaturate = drawer->desaturate;
            material = drawer->material;
            image = drawer->image;
            blend = drawer->blend;
            mask = drawer->mask;
            batch_reset(batch, material);
        }
        batch_add(batch, drawer, viewportWidth, viewportHeight,
                &projection_matrix);
    }
    batch_flush(batch, material, image, blend, mask, desaturate,
            viewportWidth, viewportHeight);
}

void
ir_render(struct ir* ir) {
    struct batch batch = {
        .verticies = ir->scratchVerticies,
        .indicies = ir->scratchIndicies,
        .attributes = ir->scratchAttributes,
    };
    for (struct l2d_target* itr = ir->targetList; itr != NULL; itr=itr->next) {
        render_api_draw_start(itr->fbo,
                i_target_scaled_width(itr),
                i_target_scaled_height(itr));
        render_api_clear_f(itr->color);

        drawDrawerList(&batch, itr->drawerList, itr->width, itr->height, NULL,
                &itr->sort_cache);
    }
    render_api_draw_start(0, ir->viewportWidth, ir->viewportHeight);
    drawDrawerList(&batch, ir->drawerList,
            ir->viewportWidth, ir->viewportHeight, ir->translate,
            &ir->sort_cache);

    // write back the scratch buffer pointers, as they might have been
    // reallocated:
    ir->scratchVerticies = batch.verticies;
    ir->scratchIndicies = batch.indicies;
    ir->scratchAttributes = batch.attributes;
}

