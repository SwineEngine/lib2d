#ifndef __LIB2D_RENDER_API__
#define __LIB2D_RENDER_API__

#include "lib2d.h"
#include "renderer.h"
#include "image_bank.h"
#include <stdint.h>

#define SHADER_EXTERNAL_IMAGE (1 << 0)
#define SHADER_MASK (1 << 1)
#define SHADER_DESATURATE (1 << 2)

#define SHADER_VARIANT_COUNT ((SHADER_EXTERNAL_IMAGE \
            | SHADER_MASK \
            | SHADER_DESATURATE \
            )+ 1)

struct shader;
struct material;

struct shader_handles {
    int32_t id;
    int32_t positionHandle;
    int32_t texCoordHandle;
    int32_t miscAttrib;
    int32_t colorAttrib;
    int32_t textureHandle;
    int32_t texturePixelSizeHandle;
    int32_t miscAnimatingHandle;
    int32_t maskTexture;
    int32_t maskTextureCoordMat;
    int32_t eyePos;
};

struct material_handles {
    bool invalid;
    int32_t* imageUniforms;
    int32_t* podUniforms;
    int32_t* attributes;
};

struct material_attribute {
    l2d_ident name;
    unsigned long size;
};

struct material_attribute*
render_api_get_attributes(struct material*, int* count);

struct material_pod_uniform;
struct material_image_uniform {
    struct l2d_image* image;
    const char* name;
};

enum texture_type {
    TEXTURE_2D,
    TEXTURE_EXTERNAL_OES,
};

uint32_t
render_api_texture_new(enum texture_type type);

void
render_api_texture_delete(uint32_t native_ptr);

void
render_api_texture_bind(enum texture_type, uint32_t native_ptr,
        int32_t handle, int texture_slot,
        int pixel_size_shader_handle, int w, int h);


struct render_api_upload_info {
    void* data;
    int width, height;
    enum texture_type texture_type;
    enum l2d_image_format format;
    uint32_t native_ptr;
    bool clamp;
};

void
render_api_texture_upload(struct render_api_upload_info*);

void
render_api_get_viewport(int[4]);

void
render_api_draw_batch(struct batch*, struct shader_handles*, struct
        material*, struct material_handles*, enum l2d_blend);

void
render_api_set_vec(int32_t handle, float x, float y, float z, float w);

void
render_api_draw_start(int fbo_target, int viewport_w, int viewport_h);

enum shader_type {
    SHADER_DEFAULT,
    SHADER_BLUR_H,
    SHADER_BLUR_V,
    SHADER_UPSAMPLE,
    SHADER_SINGLE_CHANNEL,
};

struct shader*
render_api_load_shader(enum shader_type);

struct material*
render_api_material_new(struct shader*);

/*
void
material_set_image(struct material* material,
        const char* name, struct l2d_image* image);
*/

void
render_api_material_set_float_v(struct material* material,
        const char* name, int count, float* floats);

void
render_api_material_enable_vertex_data(struct material* material,
        l2d_ident attribute, int size);

void
render_api_material_use(struct material* m, unsigned int shader_variant,
        struct shader_handles** sh, struct material_handles** mh,
        int* next_texture_slot);

void
render_api_clear();

#endif
