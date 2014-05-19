#include "lib2d.h"
#include "effect.h"
#include "template.h"
#include "stretchy_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define EXPORTED __attribute__((visibility("default")))

static
int
map_input(struct l2d_effect* e, int input) {
    if (input == -1)
        return sbcount(e->components);
    return input;
}

static
void
get_comp(struct l2d_effect* e, int index, char* r, int input_index) {
    assert(index > -1);

    // For now, if a component has multiple inputs, they must both be targets.
    assert(input_index == 1);

    if (index == 0) {
        strcpy(r, "tex"); // Sampled main texture
    } else
        sprintf(r, "e_%d", index);
}

/**
 * get_target is called when an effect needs to sample from the result of
 * previous effect components. It will mark the component at input as needing
 * to be the end of a stage (render target.)
 */
static
void
get_target(struct l2d_effect* e, int input, char* r, int input_index) {
    assert(input > -1);

    // Because of the way render targets work, the sampler is always "texture"
    // for the first input.
    if (input_index == 1) {
        strcpy(r, "texture");
    } else {
        strcpy(r, "texture2");
    }

    if (input > 0) {
        e->components[input-1].stage_end = true;
    }
}

static
struct l2d_effect_component*
new_comp(struct l2d_effect* e, int input, int input2, char* name) {
    assert(input > -1);
    int id = sbcount(e->components)+1;
    get_comp(e, id, name, 1);
    struct l2d_effect_component* c = sbadd(e->components, 1);
    c->id = id;
    c->inputs[0] = input;
    c->inputs[1] = input2;
    c->source = NULL;
    c->stage_end = false;
    c->stage_i = -1;
    return c;
}

EXPORTED
struct l2d_effect*
l2d_effect_new() {
    struct l2d_effect* e = (struct l2d_effect*)malloc(sizeof(struct l2d_effect));
    static int id = 0;
    e->id = id;
    id ++;
    e->stages = NULL;
    e->components = NULL;
    return e;
}

EXPORTED
void
l2d_effect_delete(struct l2d_effect* e) {
    sbfree(e->stages);

    sbforeachp(struct l2d_effect_component* c, e->components)
        free(c->source);
    sbfree(e->components);

    free(e);
}

EXPORTED
void
l2d_effect_update_stages(struct l2d_effect* e) {
    // TODO rebuild stages if needed
    if (e->stages) return;

    // The last component will be the final stage
    sblast(e->components).stage_end = true;

    int j = 0;
    sbforeachp(struct l2d_effect_component* c, e->components) {
        if (c->stage_end) {
            int stage_i = sbcount(e->stages);
            struct l2d_effect_stage* s = sbadd(e->stages, 1);
            s->id = stage_i;
            s->effect = e;
            s->stage_dep[0] = -1; // This will be overriden if a dependacy arrives at another stage.
            s->stage_dep[1] = -1;
            s->components[0] = j;
            c->stage_i = stage_i;
            s->num_components = 1;
            int num = (c->inputs[0] == c->inputs[1]) ? 1 : 2;
            for (int k=0; k<num; k++) {
                struct l2d_effect_component* itr = c;
                while (itr->inputs[k]) {
                    int i = itr->inputs[k]-1;
                    assert(i>=0 && i<sbcount(e->components));
                    itr = &e->components[i];
                    if (itr->stage_end) {
                        // We temp store which component instead of stage since we
                        // don't know which stage it will be in yet.
                        s->stage_dep[k] = i;
                        break;
                    } else {
                        s->components[s->num_components] = i;
                        s->num_components ++;
                        itr->stage_i = stage_i;
                        assert(s->num_components < 32);
                    }
                }
            }
        }
        j++;
    }

    // find stage_deps
    sbforeachp(struct l2d_effect_stage* s, e->stages) {
        for (int k=0; k<2; k++) {
            // TODO No stage dep value as -1?
            if (s->stage_dep[k] == -1) {
                s->stage_dep[k] = 0;
            } else {
                s->stage_dep[k] = e->components[s->stage_dep[k]].stage_i+1;
            }
        }
    }
}

EXPORTED
void
l2d_effect_color_matrix(struct l2d_effect* e, int input, float t[16]) {
    input = map_input(e, input);
    char comp[16];
    struct l2d_effect_component* c = new_comp(e, input, input, comp);
    char inp[16]; get_comp(e, input, inp, 1);

    char w[2048];
    int n = sprintf(w,
            "// color matrix\n"
            "mat4 %s_mat = mat4(%f,%f,%f,%f, %f,%f,%f,%f, %f,%f,%f,%f, %f,%f,%f,%f);\n"
            "vec4 %s = %s_mat * %s;\n",
            comp,
            t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], t[10], t[11], t[12], t[13], t[14], t[15],
            comp, comp, inp);
    c->source = malloc(n+1);
    memcpy(c->source, w, n+1);
}

EXPORTED
void
l2d_effect_fractal_noise(struct l2d_effect* e, int input,
        float frequency_x, float frequency_y,
        int octaves, int seed) {
}

EXPORTED
void
l2d_effect_convolve_matrix(struct l2d_effect* e, int input, float k[9]) {
    input = map_input(e, input);
    char comp[16];
    struct l2d_effect_component* c = new_comp(e, input, input, comp);

    char target[16]; get_target(e, input, target, 1);
    struct template_var vars[] = {
        {"COMP", comp},
        {"INP_TEX", target},
        {"INP", "tex"},
        {0,0}};

#define LOOKUP(NAME, X, Y) "vec3 COMP_" #NAME " = texture2D(INP_TEX, texCoord_v + vec2(" #X "," #Y ")*texturePixelSize).rgb;\n"
    char w[2048];
    sprintf(w,
            "// convolve matrix.\n"
            LOOKUP(b,  0,  -1)
            LOOKUP(lb, -1, -1)
            LOOKUP(l,  -1, 0)
            LOOKUP(lt, -1, 1)
            LOOKUP(t,  0,  1)
            LOOKUP(rt, 1,  1)
            LOOKUP(r,  1,  0)
            LOOKUP(rb, 1,  -1)
            "vec3 COMP_3 = COMP_lt*%f + COMP_t*%f + COMP_rt*%f;\n"
            "COMP_3 += COMP_l*%f + INP.rgb*%f + COMP_r*%f;\n"
            "COMP_3 += COMP_lb*%f + COMP_b*%f + COMP_rb*%f;\n"
            "vec4 COMP = vec4(COMP_3.rgb, INP.a);\n",
            k[0], k[1], k[2],
            k[3], k[4], k[5],
            k[6], k[7], k[8]);
#undef LOOKUP
    c->source = replace_vars(vars, w, "");
}

EXPORTED
void
l2d_effect_erode(struct l2d_effect* e, int input) {
    input = map_input(e, input);
    char comp[16];
    struct l2d_effect_component* c = new_comp(e, input, input, comp);
    char target[16]; get_target(e, input, target, 1);
    struct template_var vars[] = {
        {"COMP", comp},
        {"INP_TEX", target},
        {"INP", "tex"},
        {0,0}};

#define LOOKUP(X, Y) "COMP = min(texture2D(INP_TEX, texCoord_v + vec2(" #X "," #Y ")*texturePixelSize), COMP);\n"
    char* w =
            "// erode matrix.\n"
            "vec4 COMP = INP;\n"
            LOOKUP(0,  -1)
            LOOKUP(-1, -1)
            LOOKUP(-1, 0)
            LOOKUP(-1, 1)
            LOOKUP(0,  1)
            LOOKUP(1,  1)
            LOOKUP(1,  0)
            LOOKUP(1,  -1);
#undef LOOKUP
    c->source = replace_vars(vars, w, "");
}

EXPORTED
void
l2d_effect_dilate(struct l2d_effect* e, int input) {
    input = map_input(e, input);
    char comp[16];
    struct l2d_effect_component* c = new_comp(e, input, input, comp);
    char target[16]; get_target(e, input, target, 1);
    struct template_var vars[] = {
        {"COMP", comp},
        {"INP_TEX", target},
        {"INP", "tex"},
        {0,0}};

#define LOOKUP(X, Y) "COMP = max(texture2D(INP_TEX, texCoord_v + vec2(" #X "," #Y ")*texturePixelSize), COMP);\n"
    const char* w =
            "// dilate matrix.\n"
            "vec4 COMP = INP;\n"
            LOOKUP(0,  -1)
            LOOKUP(-1, -1)
            LOOKUP(-1, 0)
            LOOKUP(-1, 1)
            LOOKUP(0,  1)
            LOOKUP(1,  1)
            LOOKUP(1,  0)
            LOOKUP(1,  -1);
#undef LOOKUP
    c->source = replace_vars(vars, w, "");
}

EXPORTED
void
l2d_effect_blur_v(struct l2d_effect* e, int input) {
    input = map_input(e, input);
    char comp[16];
    struct l2d_effect_component* c = new_comp(e, input, input, comp);
    char target[16]; get_target(e, input, target, 1);
    struct template_var vars[] = {
        {"COMP", comp},
        {"INP_TEX", target},
        {"INP", "tex"},
        {0,0}};

    const char* w =
            "// vertical blur\n"
            "vec4 COMP = vec4(0.0);\n"
#define S(OFFSET, WEIGHT) "COMP += texture2D(texture, texCoord_v+"\
            "vec2(0., texturePixelSize.y*" #OFFSET "))*" #WEIGHT ";\n"
        S(-4.0,0.027630550638898826)
        S(-3.0,0.0662822452863612)
        S(-2.0,0.1238315368057753)
        S(-1.0,0.18017382291138087)
        S(0.0,0.20416368871516752)
        S(1.0,0.18017382291138087)
        S(2.0,0.1238315368057753)
        S(3.0,0.0662822452863612)
        S(4.0,0.027630550638898826);
#undef S
    c->source = replace_vars(vars, w, "");
}

EXPORTED
void
l2d_effect_blur_h(struct l2d_effect* e, int input) {
    input = map_input(e, input);
    char comp[16];
    struct l2d_effect_component* c = new_comp(e, input, input, comp);
    char target[16]; get_target(e, input, target, 1);
    struct template_var vars[] = {
        {"COMP", comp},
        {"INP_TEX", target},
        {"INP", "tex"},
        {0,0}};

    const char* w =
            "// horizontal blur\n"
            "vec4 COMP = vec4(0.0);\n"
#define S(OFFSET, WEIGHT) "COMP += texture2D(texture, texCoord_v+"\
            "vec2(texturePixelSize.x*" #OFFSET ", 0.))*" #WEIGHT ";\n"
        S(-4.0,0.027630550638898826)
        S(-3.0,0.0662822452863612)
        S(-2.0,0.1238315368057753)
        S(-1.0,0.18017382291138087)
        S(0.0,0.20416368871516752)
        S(1.0,0.18017382291138087)
        S(2.0,0.1238315368057753)
        S(3.0,0.0662822452863612)
        S(4.0,0.027630550638898826);
#undef S
    c->source = replace_vars(vars, w, "");
}

EXPORTED
void
l2d_effect_blend(struct l2d_effect* e, int input, int input2,
        enum l2d_effect_blend mode) {
    input = map_input(e, input);
    input2 = map_input(e, input2);
    char comp[16];
    struct l2d_effect_component* c = new_comp(e, input, input2, comp);
    char inp[16]; get_target(e, input, inp, 1);
    char inp2[16]; get_target(e, input2, inp2, 2);
    struct template_var vars[] = {
        {"COMP", comp},
        {"INP2", inp2},
        {"INP", inp},
        {0,0}};
    const char* w;
    switch (mode)  {
        case l2d_EFFECT_BLEND_MULT:
            w = "// blend mult\nvec4 COMP = texture2D(INP, texCoord_v) * texture2D(INP2, texCoord_v);\n";
            break;
        default:
            assert(false);
    }
    c->source = replace_vars(vars, w, "");
}
