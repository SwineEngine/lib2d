#include "gl.h"
#include "render_api.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stretchy_buffer.h"
#include "image_bank.h"

#define MAX_MATERIAL_IMAGE_UNIFORMS 7
struct material {
    struct shader* shader;

    struct material_image_uniform imageUniforms[MAX_MATERIAL_IMAGE_UNIFORMS];
    unsigned long imageUniformCount;

    struct material_pod_uniform *podUniforms; // stretchy buffer

    struct material_attribute* attributes; // stretchy buffer

    struct material_handles handles[SHADER_VARIANT_COUNT];
};

static
GLuint
to_gl_type(enum texture_type texture_type) {
    switch (texture_type) {
    case TEXTURE_2D:
        return GL_TEXTURE_2D;
#ifdef GLES
#ifndef NO_EXTERNAL_OES
    case TEXTURE_EXTERNAL_OES:
        return GL_TEXTURE_EXTERNAL_OES;
#endif
#endif
    default:
        assert(false);
    }
}


uint32_t
render_api_texture_new(enum texture_type type) {
    GLuint texid = 0;
    glGenTextures(1, &texid);
    return texid;
}

void
render_api_texture_delete(uint32_t native_ptr) {
    glDeleteTextures(1, &native_ptr);
}

void
render_api_texture_bind(enum texture_type texture_type,
        uint32_t native_ptr, int32_t handle, int texture_slot,
        int pixel_size_shader_handle, int w, int h) {
    glUniform1i(handle, texture_slot);
    glActiveTexture(GL_TEXTURE0+texture_slot);
    glBindTexture(to_gl_type(texture_type), native_ptr);

    // TODO This could be done in a better place
    if (pixel_size_shader_handle != -1) {
        glUniform2f(pixel_size_shader_handle, 1.f/w, 1.f/h);
    }
}

void
render_api_texture_upload(struct render_api_upload_info* u) {
    GLuint type = to_gl_type(u->texture_type);
    glBindTexture(type, u->native_ptr);
    if (u->clamp) {
        glTexParameteri(type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum glformat = GL_RGBA;
    GLenum gltype = GL_UNSIGNED_BYTE;

    switch (u->format) {
    case l2d_IMAGE_FORMAT_RGBA_8888:
        glformat = GL_RGBA;
        gltype = GL_UNSIGNED_BYTE;
        break;
    case l2d_IMAGE_FORMAT_RGB_888:
        glformat = GL_RGB;
        gltype = GL_UNSIGNED_BYTE;
        break;
    case l2d_IMAGE_FORMAT_RGB_565:
        glformat = GL_RGB;
        gltype = GL_UNSIGNED_SHORT_5_6_5;
        break;
    case l2d_IMAGE_FORMAT_A_8:
        glformat = GL_ALPHA;
        gltype = GL_UNSIGNED_BYTE;
        break;
    default:
        assert(false);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(type, 0, glformat, u->width, u->height, 0, glformat, gltype,
            u->data);
}


void
render_api_get_viewport(int res[4]) {
    glGetIntegerv(GL_VIEWPORT, res);
}

void
render_api_draw_start(int fbo_target, int viewport_w, int viewport_h) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_target);
    glViewport(0,0, viewport_w, viewport_h);
    glDisable(GL_CULL_FACE);
}

void
render_api_draw_batch(struct batch* batch,
        struct shader_handles* shader,
        struct material* material, struct material_handles* h,
        enum l2d_blend blend) {
    switch (blend) {
    case l2d_BLEND_DISABLED:
        glDisable(GL_BLEND);
        break;
    case l2d_BLEND_DEFAULT:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        break;
    case l2d_BLEND_PREMULT:
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        break;
    default:
        assert(false);
    }

    struct vertex* v = batch->verticies;

    glVertexAttribPointer(shader->positionHandle,
            4, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
            &v[0].position[0]);
    glVertexAttribPointer(shader->texCoordHandle,
            2, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
            &v[0].texCoord[0]);
    glVertexAttribPointer(shader->colorAttrib,
            4, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
            &v[0].color[0]);
    glEnableVertexAttribArray(shader->positionHandle);
    glEnableVertexAttribArray(shader->texCoordHandle);
    glEnableVertexAttribArray(shader->colorAttrib);

    if (shader->miscAttrib != -1) {
        glVertexAttribPointer(shader->miscAttrib,
                4, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
                &v[0].misc[0]);
        glEnableVertexAttribArray(shader->miscAttrib);
    }

    for (size_t i=0; i<sbcount(material->attributes); i++) {
        GLint handle = h->attributes[i];
        if (handle == -1)
            continue;
        size_t size = batch->attributes[i].size;
        float* data = batch->attributes[i].data;
        glVertexAttribPointer(handle, size, GL_FLOAT, GL_FALSE,
                sizeof(float)*size, data);
        glEnableVertexAttribArray(handle);
    }

    glDrawElements(GL_TRIANGLES, batch->indexCount,
            GL_UNSIGNED_SHORT, batch->indicies);

    glDisableVertexAttribArray(shader->positionHandle);
    glDisableVertexAttribArray(shader->texCoordHandle);
    if (shader->miscAttrib != -1)
        glDisableVertexAttribArray(shader->miscAttrib);
    glDisableVertexAttribArray(shader->colorAttrib);

    for (size_t i=0; i<sbcount(material->attributes); i++) {
        GLint handle = h->attributes[i];
        if (handle == -1)
            continue;
        glDisableVertexAttribArray(handle);
    }
}

void
render_api_set_vec(int32_t handle, float x, float y, float z, float w) {
    glUniform4f(handle, x, y, z, w);
}


//
// Shader/Material code
//

struct material_pod_uniform {
    float floats[4];
    int size; // 1 for float, 2 for vec2, etc.
    const char* name;
};


static
GLuint
compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1) {
        char * log = malloc((logLength)*sizeof(char));
        glGetShaderInfoLog(shader, logLength, NULL, log);
        printf("Shader error:\n%s\nSource:\n%s", log, source);
        free(log);
    }
    return shader;
}

static const char* defaultVertexSource =
        "attribute vec4 position;\n"
        "attribute vec2 texCoord;\n"
        "attribute vec4 miscAttrib;\n"
        "attribute vec4 colorAttrib;\n"
        "varying vec2 texCoord_v;\n"
        "varying float alpha_v;\n"
        "varying vec4 color_v;\n"
        "MASK_VERTEX_HEAD"
        "DESATURATE_VERTEX_HEAD"
        "void main() {\n"
        "    texCoord_v = texCoord;\n"
        "    color_v = vec4(colorAttrib.rgb, 1.0);\n"
        "    alpha_v = colorAttrib.a;\n"
        "    gl_Position = position;\n"
        "MASK_VERTEX_BODY"
        "DESATURATE_VERTEX_BODY"
        "}\n";
static const char* defaultFragmentSource =
#ifdef GLES
        "precision mediump float;\n"
#endif
        "varying vec2 texCoord_v;\n"
        "varying float alpha_v;\n"
        "varying vec4 color_v;\n"
        "uniform SAMPLER0 texture;\n"
        "MASK_FRAGMENT_HEAD"
        "DESATURATE_FRAGMENT_HEAD"
        "void main() {\n"
        "    gl_FragColor = texture2D(texture, texCoord_v)*color_v*vec4(1.0, 1.0, 1.0, alpha_v);\n"
        "MASK_FRAGMENT_BODY"
        "DESATURATE_FRAGMENT_BODY"
        "}\n";
static const char* premultFragmentSource =
#ifdef GLES
        "precision mediump float;\n"
#endif
        "varying vec2 texCoord_v;\n"
        "varying float alpha_v;\n"
        "varying vec4 color_v;\n"
        "uniform SAMPLER0 texture;\n"
        "MASK_FRAGMENT_HEAD"
        "DESATURATE_FRAGMENT_HEAD"
        "void main() {\n"
        "    gl_FragColor = texture2D(texture, texCoord_v)*color_v*alpha_v;\n"
        "MASK_FRAGMENT_BODY"
        "DESATURATE_FRAGMENT_BODY"
        "}\n";
static const char* singleChannelFragmentSource =
#ifdef GLES
        "precision mediump float;\n"
#endif
        "varying vec2 texCoord_v;\n"
        "varying float alpha_v;\n"
        "varying vec4 color_v;\n"
        "uniform SAMPLER0 texture;\n"
        "MASK_FRAGMENT_HEAD"
        "DESATURATE_FRAGMENT_HEAD"
        "void main() {\n"
        "    gl_FragColor = texture2D(texture, texCoord_v).a*color_v*alpha_v;\n"
        "MASK_FRAGMENT_BODY"
        "DESATURATE_FRAGMENT_BODY"
        "}\n";

static const char* blurVertexSource =
        "attribute vec4 position;\n"
        "attribute vec2 texCoord;\n"
        "attribute vec4 miscAttrib;\n"
        "varying vec2 texCoord_v;\n"
        "varying float alpha_v;\n"
        "MASK_VERTEX_HEAD"
        "DESATURATE_VERTEX_HEAD"
        "void main() {\n"
        "    texCoord_v = vec2(texCoord.x, 1.-texCoord.y);\n"
        "    gl_Position = position;\n"
        "MASK_VERTEX_BODY"
        "DESATURATE_VERTEX_BODY"
        "}\n";
static const char* fragmentSourceH =
#ifdef GLES
        "precision mediump float;\n"
#endif
        "varying vec2 texCoord_v;\n"
        "uniform sampler2D texture;\n"
        "uniform vec2 texturePixelSize;\n"
        "void main() {\n"
        "    vec4 color = vec4(0.,0.,0.,0.);\n"
#define S(OFFSET, WEIGHT) "color += texture2D(texture, texCoord_v+"\
            "vec2(texturePixelSize.x*" #OFFSET ", 0.))*" #WEIGHT ";\n"
        S(-4.0,0.027630550638898826)
        S(-3.0,0.0662822452863612)
        S(-2.0,0.1238315368057753)
        S(-1.0,0.18017382291138087)
        S(0.0,0.20416368871516752)
        S(1.0,0.18017382291138087)
        S(2.0,0.1238315368057753)
        S(3.0,0.0662822452863612)
        S(4.0,0.027630550638898826)
#undef S
        "    gl_FragColor = color;\n"
        "}\n";
static const char* fragmentSourceV =
#ifdef GLES
        "precision mediump float;\n"
#endif
        "varying vec2 texCoord_v;\n"
        "uniform sampler2D texture;\n"
        "uniform vec2 texturePixelSize;\n"
        "void main() {\n"
        "    vec4 color = vec4(0.,0.,0.,0.);\n"
#define S(OFFSET, WEIGHT) "color += texture2D(texture, texCoord_v+"\
            "vec2(0., texturePixelSize.y*" #OFFSET "))*" #WEIGHT ";\n"
        S(-4.0,0.027630550638898826)
        S(-3.0,0.0662822452863612)
        S(-2.0,0.1238315368057753)
        S(-1.0,0.18017382291138087)
        S(0.0,0.20416368871516752)
        S(1.0,0.18017382291138087)
        S(2.0,0.1238315368057753)
        S(3.0,0.0662822452863612)
        S(4.0,0.027630550638898826)
#undef S
        "    gl_FragColor = color;\n"
        "}\n";

static const char* fragmentSourceUpsample =
#ifdef GLES
        "precision mediump float;\n"
#endif
        "varying vec2 texCoord_v;\n"
        "varying float alpha_v;\n"
        "uniform sampler2D texture;\n"
        "uniform vec2 texturePixelSize;\n"
        "MASK_FRAGMENT_HEAD"
        "DESATURATE_VERTEX_HEAD"
        "void main() {\n"
        "    gl_FragColor = texture2D(texture, texCoord_v);\n"
        "MASK_FRAGMENT_BODY"
        "DESATURATE_FRAGMENT_BODY"
        "}\n";



struct template_var {
    const char* key;
    const char* value;
};

static struct template_var mask_vars[] = {
    {"MASK_VERTEX_HEAD",
        "uniform vec4 eyePos;\n"
        "varying vec2 maskTextureCoord;\n"
        "uniform mat4 maskTextureCoordMat;\n"},
    {"MASK_VERTEX_BODY",
        "vec4 mask_p1 = maskTextureCoordMat*eyePos;\n"
        "vec4 mask_p2 = maskTextureCoordMat*vec4(position.xy/position.w,0.,1.);\n"
        "vec4 mask_ray = mask_p2 - mask_p1;\n"
        "maskTextureCoord = ((-mask_p1.z*mask_ray.xy)/mask_ray.z+mask_p1.xy);\n"
    },
    {"MASK_FRAGMENT_HEAD",
        "varying vec2 maskTextureCoord;\n"
        "uniform sampler2D maskTexture;\n"},
    {"MASK_FRAGMENT_BODY",
        "gl_FragColor *= texture2D(maskTexture, maskTextureCoord).a;\n"
    },
    {0,0}
};

static struct template_var desaturate_vars[] = {
    {"DESATURATE_VERTEX_HEAD",
        "varying float desaturate_v;\n"
    },
    {"DESATURATE_VERTEX_BODY",
        "desaturate_v=miscAttrib[0];\n"
    },
    {"DESATURATE_FRAGMENT_HEAD",
        "varying float desaturate_v;\n"
    },
    {"DESATURATE_FRAGMENT_BODY",
        "gl_FragColor.xyz = mix(gl_FragColor.xyz, "
            "vec3(dot(vec3(0.3, 0.59, 0.11), gl_FragColor.xyz)), "
            "desaturate_v);\n"
    },
    {0,0}
};

struct shader {
    l2d_ident name;
    struct shader_handles handles[SHADER_VARIANT_COUNT];
    const char* vertexSource;
    const char* fragmentSource;
};

static
char*
template(struct template_var* vars, const char* source, const char* prefix) {
    // figure out the total size needed for the buffer
    size_t len = strlen(source) + strlen(prefix);
    for (size_t i=0; vars[i].key; i++) {
        len += strlen(vars[i].key) + strlen(vars[i].value);
    }

    char* result = malloc(len+1);
    strcpy(result, prefix);
    strcat(result, source);

    for (size_t i=0; vars[i].key; i++) {
        char* found = strstr(result, vars[i].key);
        if (found) {
            size_t len_key = strlen(vars[i].key);
            size_t len_value = strlen(vars[i].value);
            memmove(found+len_value, found+len_key, strlen(found+len_key)+1);
            memcpy(found, vars[i].value, len_value);
        }
    }

    return result;
}

static
void
set_var(struct template_var* vars, const char* key, const char* value) {
    for (size_t i=0; vars[i].key; i++) {
        if (strcmp(vars[i].key, key) == 0) {
            vars[i].value = value;
            return;
        }
    }
    assert(false);
}

static
void
update_vars(struct template_var* dest, struct template_var const* src) {
    for (size_t i=0; src[i].key; i++) {
        set_var(dest, src[i].key, src[i].value);
    }
}

static
void
loadProgram(struct shader* program, unsigned int variant) {
    const char* fragmentPrefix = "";

    struct template_var vars[] = {
        {"SAMPLER0","sampler2D"},
        {"MASK_VERTEX_HEAD",""},
        {"MASK_VERTEX_BODY",""},
        {"MASK_FRAGMENT_HEAD",""},
        {"MASK_FRAGMENT_BODY",""},
        {"DESATURATE_VERTEX_HEAD",""},
        {"DESATURATE_VERTEX_BODY",""},
        {"DESATURATE_FRAGMENT_HEAD",""},
        {"DESATURATE_FRAGMENT_BODY",""},
        {0,0}};

    if (variant & SHADER_EXTERNAL_IMAGE) {
        fragmentPrefix = "#extension GL_OES_EGL_image_external : require\n";
        set_var(vars, "SAMPLER0", "samplerExternalOES");
    }

    if (variant & SHADER_MASK) {
        update_vars(vars, mask_vars);
    }

    if (variant & SHADER_DESATURATE) {
        update_vars(vars, desaturate_vars);
    }

    char* fragSource = template(vars, program->fragmentSource,
            fragmentPrefix);

    char* vertSource = template(vars, program->vertexSource, "");

    struct shader_handles* h = &program->handles[variant];

    h->id = glCreateProgram();
    glAttachShader(h->id,
            compileShader(GL_VERTEX_SHADER, vertSource));
    glAttachShader(h->id,
            compileShader(GL_FRAGMENT_SHADER, fragSource));
    glLinkProgram(h->id);
    glUseProgram(h->id);

    GLint logLength;
    glGetProgramiv(h->id, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1) {
        char * log = malloc((logLength)*sizeof(char));
        glGetProgramInfoLog(h->id, logLength, NULL, log);
        printf("Linker error:\n%s", log);
        free(log);
    }

    h->positionHandle = glGetAttribLocation(h->id, "position");
    h->texCoordHandle = glGetAttribLocation(h->id, "texCoord");
    h->miscAttrib = glGetAttribLocation(h->id, "miscAttrib");
    h->colorAttrib = glGetAttribLocation(h->id, "colorAttrib");
    h->textureHandle = glGetUniformLocation(h->id, "texture");
    h->texturePixelSizeHandle = glGetUniformLocation(h->id,
            "texturePixelSize");
    h->miscAnimatingHandle = glGetUniformLocation(h->id,
            "miscAnimating");
    h->maskTexture = glGetUniformLocation(h->id, "maskTexture");
    h->maskTextureCoordMat = glGetUniformLocation(h->id,
            "maskTextureCoordMat");
    h->eyePos = glGetUniformLocation(h->id, "eyePos");

    free(vertSource);
    free(fragSource);
}

static
void
material_handles_init(struct material_handles* h) {
    h->invalid = true;
    h->imageUniforms = NULL;
    h->podUniforms = NULL;
    h->attributes = NULL;
}

static
void
material_invalidate_handles(struct material* m) {
    for (unsigned int i=0; i<SHADER_VARIANT_COUNT; i++) {
        m->handles[i].invalid = true;
    }
}

static
void
materialRefreshShaderHandlesVariant(struct material* m,
        struct shader_handles* shader,
        struct material_handles* h) {
    sbempty(h->imageUniforms);
    for (int i=0; i<m->imageUniformCount; i++) {
        sbpush(h->imageUniforms, glGetUniformLocation(shader->id,
                    m->imageUniforms[i].name));
    }

    sbempty(h->podUniforms);
    for (int i=0; i<sbcount(m->podUniforms); i++) {
        sbpush(h->podUniforms, glGetUniformLocation(shader->id,
                    m->podUniforms[i].name));
    }

    sbempty(h->attributes);
    for (int i=0; i<sbcount(m->attributes); i++) {
        struct material_attribute* entry = &m->attributes[i];
        sbpush(h->attributes, glGetAttribLocation(shader->id,
                l2d_ident_as_char(entry->name)));
    }

    h->invalid = false;
}

static
void
materialSetUpUniforms(struct material* m, unsigned int shader_variant,
        int* next_texture_slot) {
    struct material_handles* h = &m->handles[shader_variant];

    for (int i=0; i<m->imageUniformCount; i++) {
        struct material_image_uniform* entry = &m->imageUniforms[i];
        // TODO support passing pixel sizes for material textures
        ib_image_bind(entry->image, -1, h->imageUniforms[i], *next_texture_slot);
        (*next_texture_slot)++;
    }

    void (*uniformAPICalls[])(GLint, GLsizei, const GLfloat*)  = {
            glUniform1fv, glUniform2fv, glUniform3fv, glUniform4fv};
    for (int i=0; i<sbcount(m->podUniforms); i++) {
        struct material_pod_uniform* entry = &m->podUniforms[i];
        uniformAPICalls[entry->size - 1](h->podUniforms[i], 1, entry->floats);
    }
}

static
struct shader*
shader_new(const char* vertexSource, const char* fragSource) {
    struct shader* shader = malloc(sizeof(struct shader));
    memset(shader, 0, sizeof(struct shader));
    shader->vertexSource = vertexSource;
    shader->fragmentSource = fragSource;
    return shader;
}

struct material*
render_api_material_new(struct shader* shader) {
    struct material* material = malloc(sizeof(struct material));
    material->shader = shader;
    material->imageUniformCount = 0;
    material->podUniforms = NULL;
    material->attributes = NULL;
    for (unsigned int i=0; i<SHADER_VARIANT_COUNT; i++) {
        material_handles_init(&material->handles[i]);
    }
    return material;
}

struct material_attribute*
render_api_get_attributes(struct material* m, int* count) {
    *count = sbcount(m->attributes);
    return m->attributes;
}

/*
void
material_set_image(struct material* material,
        const char* name, struct l2d_image* image) {
    // First see if it already exists:
    for (int i = 0; i < material->imageUniformCount; i++) {
        struct material_image_uniform* entry = &material->imageUniforms[i];
        if (strcmp(entry->name, name) == 0) {
            entry->image = image;
            return;
        }
    }
    assert(material->imageUniformCount < MAX_MATERIAL_IMAGE_UNIFORMS);
    struct material_image_uniform* entry =
            &material->imageUniforms[material->imageUniformCount++];

    entry->image = image;
    char* newName = malloc(sizeof(char)*(strlen(name)+1));
    strcpy(newName, name);
    entry->name = newName;
    material_invalidate_handles(material);
}
*/

void
render_api_material_set_float_v(struct material* m,
        const char* name, int count, float* floats) {
    assert(count > 0);
    assert(count <= 4);
    // see if it already exists:
    for (int i=0; i<sbcount(m->podUniforms); i++) {
        struct material_pod_uniform* entry = &m->podUniforms[i];
        if (strcmp(entry->name, name) == 0) {
            memcpy(entry->floats, floats, sizeof(float)*count);
            entry->size = count;
            return;
        }
    }

    struct material_pod_uniform* entry = sbadd(m->podUniforms, 1);

    memcpy(entry->floats, floats, sizeof(float)*count);
    entry->size = count;

    char* newName = malloc(sizeof(char)*(strlen(name)+1));
    strcpy(newName, name);
    entry->name = newName;

    material_invalidate_handles(m);
}

void
render_api_material_enable_vertex_data(struct material* m,
        l2d_ident attribute, int size) {
    for (size_t i=0; i<sbcount(m->attributes); i++) {
        if (m->attributes[i].name == attribute) {
            m->attributes[i].size = size;
            return;
        }
    }
    struct material_attribute* ma = sbadd(m->attributes, 1);
    ma->name = attribute;
    ma->size = (size_t)size;
    material_invalidate_handles(m);
}


void
render_api_material_use(struct material* m, unsigned int shader_variant,
        struct shader_handles** sh, struct material_handles** mh,
        int* next_texture_slot) {

    *sh = &m->shader->handles[shader_variant];
    if ((*sh)->id == 0) {
        loadProgram(m->shader, shader_variant);
    }
    *mh = &m->handles[shader_variant];
    if ((*mh)->invalid) {
        materialRefreshShaderHandlesVariant(m, *sh, *mh);
    }

    glUseProgram((*sh)->id);
    materialSetUpUniforms(m, shader_variant, next_texture_slot);
}

struct shader*
render_api_load_shader(enum shader_type t) {
    switch (t) {
    case SHADER_DEFAULT:
        return shader_new(defaultVertexSource, defaultFragmentSource);
    case SHADER_PREMULT:
        return shader_new(defaultVertexSource, premultFragmentSource);
    case SHADER_SINGLE_CHANNEL:
        return shader_new(defaultVertexSource, singleChannelFragmentSource);
    case SHADER_BLUR_H:
        return shader_new(blurVertexSource, fragmentSourceH);
    case SHADER_BLUR_V:
        return shader_new(blurVertexSource, fragmentSourceV);
    case SHADER_UPSAMPLE:
        return shader_new(blurVertexSource, fragmentSourceUpsample);
    default:
        assert(false);
    }
}

void
render_api_clear(uint32_t color) {
    static uint32_t c = 0;
    if (color != c) {
        c = color;
        glClearColor(
            ((c>>24)&255)/255.f,
            ((c>>16)&255)/255.f,
            ((c>>8)&255)/255.f,
            (c&255)/255.f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
