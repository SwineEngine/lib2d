#include "lib2d.h"
#include "effect.h"
#include "template.h"
#include "stretchy_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static
void
get_comp(struct l2d_effect* e, int index, char* r) {
    if (index == -1)
        index = sbcount(e->components);

    if (index == 0)
        strcpy(r, "tex"); // Sampled main texture
    else
        sprintf(r, "e_%d", index);
}

struct l2d_effect*
l2d_effect_new() {
    struct l2d_effect* e = (struct l2d_effect*)malloc(sizeof(struct l2d_effect));
    static int id = 0;
    e->id = id;
    id ++;
    e->components = NULL;
    return e;
}

void
l2d_effect_delete(struct l2d_effect* e) {
    sbforeachv(char* c, e->components)
        free(c);
    sbfree(e->components);
    free(e);
}

void
l2d_effect_color_matrix(struct l2d_effect* e, int input, float t[16]) {
    char comp[16]; get_comp(e, sbcount(e->components)+1, comp);
    char inp[16]; get_comp(e, input, inp);

    char w[2048];
    int n = sprintf(w,
            "mat4 %s_mat = mat4(%f,%f,%f,%f, %f,%f,%f,%f, %f,%f,%f,%f, %f,%f,%f,%f);\n"
            "vec4 %s = %s_mat * %s;\n",
            comp,
            t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], t[10], t[11], t[12], t[13], t[14], t[15],
            comp, comp, inp);
    char* r = malloc(n+1);
    memcpy(r, w, n+1);

    sbpush(e->components, r);
}

void
l2d_effect_fractal_noise(struct l2d_effect* e, int input,
        float frequency_x, float frequency_y,
        int octaves, int seed) {
}

void
l2d_effect_convolve_matrix(struct l2d_effect* e, int input, float k[9]) {
    char comp[16]; get_comp(e, sbcount(e->components)+1, comp);
    char inp[16]; get_comp(e, input, inp);
    struct template_var vars[] = {
        {"COMP", comp},
        {"INP_TEX", "texture"}, // TODO render targets
        {"INP", inp},
        {0,0}};

#define LOOKUP(NAME, X, Y) "vec3 COMP_" #NAME " = texture2D(INP_TEX, texCoord_v + vec2(" #X "," #Y ")*texturePixelSize).rgb;\n"
    char w[2048];
    sprintf(w,
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
    char* r = replace_vars(vars, w, "");
    sbpush(e->components, r);
}
