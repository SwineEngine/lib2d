#ifndef __LIB2d_EFFECT__
#define __LIB2d_EFFECT__

#include <stdbool.h>

struct l2d_effect_component {
    int id; // shader source will be e_`id`
    int inputs[2];
    char* source;
    int stage_i; // Used when updating stages
    bool stage_end;
};

struct l2d_effect;
struct l2d_effect_stage {
    struct l2d_effect* effect;

    // Which components make up this stage. Iterate in reverse when building shader.
    // Stored as index values (not index+1 like inputs)
    int components[32];

    int num_components;

    // which other stage this one depends on. This is used so when the target
    // for this stage is created, stage_dep's target can be used as this
    // stage's drawer
    // Stored in the same format as inputs (i-1 to index, value of 0 meaning no stage.)
    int stage_dep;
};

struct l2d_effect {
    int id;
    struct l2d_effect_stage* stages; // stretchy_buffer
    struct l2d_effect_component* components; // stretchy_buffer
};

void
l2d_effect_update_stages(struct l2d_effect*);

#endif
