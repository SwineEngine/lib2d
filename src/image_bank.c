#include "image_bank.h"
#include "lib2d.h"
#include "atlas.h"
#include "nine_patch.h"
#include "render_api.h"
#include "atlas_bank.h"
#include "primitives.h"
#include "gl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



struct texture {
    int refcount;
    uint32_t native_ptr;
    int width;
    int height;
    enum texture_type textureType;
};

struct l2d_image {
    struct l2d_image_bank* ib;
    struct l2d_image* next;
    struct l2d_image** prev;
    int refcount;

    int width;
    int height;

    struct atlas_bank_entry* atlas_bank_entry;
    struct texture* texture;
    struct rect texture_region;

    struct l2d_nine_patch* nine_patch;
    struct l2d_target* renderTarget;

    bool flip_y;
    enum l2d_image_format format;
};

struct l2d_image_bank {
    // images will always be in one of these lists:
    struct l2d_image* imageList; // images upon which no action is needed
    //struct l2d_image* imageToUploadList; // needs to be uploaded

    struct pending_upload* pendingUploadList;

    struct atlas_bank* atlas_bank;
};

struct l2d_image_bank*
ib_new(void) {
    struct l2d_image_bank* ib = (struct l2d_image_bank*)malloc(sizeof(struct l2d_image_bank));
    ib->imageList = NULL;
    ib->pendingUploadList = NULL;
    ib->atlas_bank = atlas_bank_new();
    return ib;
}

static
void
image_delete_data(struct l2d_image* image) {
    if (image->texture) {
        ib_texture_decref(image->texture);
        image->texture = NULL;
    }
    image->width = 0;
    image->height = 0;
}

void
ib_delete(struct l2d_image_bank* ib) {
    struct l2d_image* image = ib->imageList;
    while (image) {
        image_delete_data(image);
        image = image->next;
    }
    if (ib->imageList) {
        // We have a list of images that have not yet been released. Just let
        // them go, they will be freed when their release comes. (Note that
        // image_release has to handle the case of 'prev' being null.)
        ib->imageList->prev = 0;
    }

    // TODO clean up pending

    atlas_bank_delete(ib->atlas_bank);

    free(ib);
}

struct l2d_image*
ib_image_new(struct l2d_image_bank* ib) {
    struct l2d_image* image =
        (struct l2d_image*)malloc(sizeof(struct l2d_image));

    image->ib = ib;
    image->next = ib->imageList;
    if (ib->imageList) {
        ib->imageList->prev = &image->next;
    }
    ib->imageList = image;
    image->prev = & ib->imageList;

    image->refcount = 0;

    image->width = image->height = 0;
    image->texture = NULL;
    image->texture_region.l = 0;
    image->texture_region.t = 0;
    image->texture_region.r = 1;
    image->texture_region.b = 1;
    image->nine_patch = NULL;
    image->renderTarget = NULL;

    image->flip_y = false;
    image->atlas_bank_entry = NULL;

    return image;
}

void
ib_image_incref(struct l2d_image* image) {
    image->refcount++;
}

void
ib_image_decref(struct l2d_image* image) {
    image->refcount--;
    if (image->refcount == 0) {
        // NOTE due to refcounting, images may be held after the imagebank has
        // been released, in which case one of them will have a null 'prev'.
        if (image->prev) {
            *image->prev = image->next;
        }
        if (image->next) {
            image->next->prev = image->prev;
        }
        image_delete_data(image);
        free(image);
    }
}

struct rect
ib_image_get_texture_region(struct l2d_image* image) {
    struct rect r = image->texture_region;
    if (image->flip_y) {
        float temp = r.t;
        r.t = r.b;
        r.b = temp;
    }
    return r;
}

void
ib_texture_incref(struct texture* tex) {
    tex->refcount++;
}

bool
ib_texture_decref(struct texture* tex) {
    tex->refcount--;
    if (tex->refcount == 0) {
        if (tex->native_ptr)
            render_api_texture_delete(tex->native_ptr);
        free(tex);
        return true;
    }
    return false;
}

void
image_release(struct l2d_image* image) {
    ib_image_decref(image);
}


struct pending_upload {
    bool clamp;
    struct texture* texture;
    enum l2d_image_format format;
    void* data;
    int width;
    int height;

    struct pending_upload* next;
};

static
void
doPendingUpload(struct pending_upload* u) {
    if (!ib_texture_decref(u->texture)) {
        if (!u->texture->native_ptr) {
            u->texture->native_ptr = render_api_texture_new(TEXTURE_2D);
            u->texture->textureType = TEXTURE_2D;
        }
        struct render_api_upload_info info = {
            .data=u->data,
            .texture_type=TEXTURE_2D,
            .native_ptr=u->texture->native_ptr,
            .clamp=u->clamp,
            .format=u->format,
            .width=u->width,
            .height=u->height
        };
        render_api_texture_upload(&info);
        u->texture->width = u->width;
        u->texture->height = u->height;
    }
    if (u->data) free(u->data);
}

void
ib_upload_pending(struct l2d_image_bank* ib) {
    if (atlas_bank_resolve(ib->atlas_bank, ib)) {
        struct l2d_image* im = ib->imageList;
        while (im) {
            if (im->atlas_bank_entry) {
                ib_image_set_texture(im, atlas_bank_get_texture(im->atlas_bank_entry));
                im->texture_region = atlas_bank_get_region(im->atlas_bank_entry);
            }
            im = im->next;
        }
    }

    while (ib->pendingUploadList) {
        struct pending_upload* u = ib->pendingUploadList;
        doPendingUpload(u);
        ib->pendingUploadList = u->next;
        free(u);
    }
}

void
texture_set_image_data(struct l2d_image_bank* ib, struct texture* tex,
        int width, int height, enum l2d_image_format format,
        void const* data, bool clamp) {
    struct pending_upload* u =
        (struct pending_upload*)malloc(sizeof(struct pending_upload));
    u->clamp = clamp;

    ib_texture_incref(tex);
    u->texture = tex;

    u->width = width;
    u->height = height;
    u->format = format;

    int bytesPerPixel = 0;
    switch (format) {
    case l2d_IMAGE_FORMAT_RGBA_8888: bytesPerPixel = 4; break;
    case l2d_IMAGE_FORMAT_RGB_888: bytesPerPixel = 3; break;
    case l2d_IMAGE_FORMAT_RGB_565: bytesPerPixel = 2; break;
    case l2d_IMAGE_FORMAT_A_8: bytesPerPixel = 1; break;
    default: assert(false);
    }

    const int size = width*height*bytesPerPixel;
    u->data = malloc(size);
    memcpy(u->data, data, size);

    u->next = ib->pendingUploadList;
    ib->pendingUploadList = u;
}

struct texture*
ib_texture_new(void) {
    struct texture* tex =
        (struct texture*)malloc(sizeof(struct texture));
    tex->refcount = 0;
    tex->native_ptr = 0;
    tex->width = 0;
    tex->height = 0;
    tex->textureType = 0;
    return tex;
}

void
image_set_data(struct l2d_image* image,
        int width, int height, enum l2d_image_format format,
        void* data, uint32_t flags) {

    image->format = format;

    int bytesPerPixel = 0;
    switch (format) {
    case l2d_IMAGE_FORMAT_RGBA_8888: bytesPerPixel = 4; break;
    case l2d_IMAGE_FORMAT_RGB_888: bytesPerPixel = 3; break;
    case l2d_IMAGE_FORMAT_RGB_565: bytesPerPixel = 2; break;
    case l2d_IMAGE_FORMAT_A_8: bytesPerPixel = 1; break;
    default: assert(false);
    }

    uint8_t* use_data = (uint8_t*)data;
    if ((flags & l2d_IMAGE_N_PATCH) && width >= 3 && height >= 3) {
        uint8_t* p = (uint8_t*)data;
        l2d_image_set_nine_patch(image, l2d_nine_patch_parse(p,
                    bytesPerPixel, width, height));
        width -= 2;
        height -= 2;
        use_data = (void*)malloc(width*height*bytesPerPixel);
        int pitch = width*bytesPerPixel;
        for (int y=0; y<height; y++) {
            memcpy((uint8_t*)use_data + y*pitch,
                    p + bytesPerPixel + (y+1)*bytesPerPixel*(width+2),
                    pitch);
        }
    }

    // TODO if size and flags are the same, and it's not atlased, reuse texture.

    image->width = width;
    image->height = height;

    if (image->texture) {
        ib_texture_decref(image->texture);
        image->texture = NULL;
    }
    if (flags & l2d_IMAGE_NO_ATLAS) {
        ib_image_set_texture(image, ib_texture_new());
        texture_set_image_data(image->ib, image->texture, width, height,
                format, use_data, !(flags & l2d_IMAGE_NO_CLAMP));
    } else {
        image->atlas_bank_entry = atlas_bank_new_entry(image->ib->atlas_bank,
                width, height, use_data, format, ATLAS_ENTRY_EXTRUDE_BORDER);
    }

    if (use_data != data) {
        free(use_data);
    }
}

void
l2d_image_set_nine_patch(struct l2d_image* image,
        struct l2d_nine_patch* patch) {
    image->nine_patch = patch;
}

struct l2d_nine_patch*
l2d_image_get_nine_patch(struct l2d_image* image) {
    return image->nine_patch;
}

void
image_set_flip_y(struct l2d_image* image, bool flip) {
    image->flip_y = flip;
}

void
ib_image_bind(struct l2d_image* image, int pixelSizeUniform, int32_t handle, int texture_slot) {
    if (image->texture) {
        render_api_texture_bind(image->texture->textureType,
                image->texture->native_ptr,
                handle, texture_slot,
                pixelSizeUniform,
                image->texture->width, image->texture->height);
    }
}

L2D_EXPORTED
void
l2d_image_bind(struct l2d_image* image, int32_t handle, int texture_slot) {
    ib_image_bind(image, -1, handle, texture_slot);
}

void
ib_image_set_texture(struct l2d_image* image, struct texture* tex) {
    struct texture* oldTex = image->texture;
    ib_texture_incref(tex);
    image->texture = tex;
    if (oldTex) {
        // decref after reasigning in case it is the same one.
        ib_texture_decref(oldTex);
    }
}

void
ib_image_setAsRenderTarget(struct l2d_image* image,
        struct l2d_target* target, int width, int height) {
    image->renderTarget = target;

    image->width = width;
    image->height = height;

    ib_image_set_texture(image, ib_texture_new());

    struct pending_upload* u = malloc(sizeof(struct pending_upload));
    u->clamp = true;

    ib_texture_incref(image->texture);
    u->texture = image->texture;

    u->width = width;
    u->height = height;
    u->format = l2d_IMAGE_FORMAT_RGBA_8888;

    u->data = NULL;

    u->next = image->ib->pendingUploadList;
    image->ib->pendingUploadList = u;
}

bool
ib_image_same_texture(struct l2d_image* lhs, struct l2d_image* rhs) {
    if (lhs == NULL && rhs == NULL) return true;
    if (lhs == NULL || rhs == NULL) return false;
    return lhs->texture == rhs->texture;
}

int
image_sort_compare(struct l2d_image* lhs, struct l2d_image* rhs) {
    if (lhs->texture == rhs->texture) {
        return 0;
    }
    return lhs->texture - rhs->texture;
}

int
ib_image_get_width(struct l2d_image* image) {
    return image->width;
}

int
ib_image_get_height(struct l2d_image* image) {
    return image->height;
}


int
image_get_width(struct l2d_image* image) {
    return (int)ib_image_get_width(image);
}

int
image_get_height(struct l2d_image* image) {
    return (int)ib_image_get_height(image);
}


enum l2d_image_format
ib_image_format(struct l2d_image* im) {
    return im->format;
}

bool
ib_image_bind_framebuffer_texture(struct l2d_image* image) {
    // TODO abstract GL
    assert(image->texture);
    if (image->texture->native_ptr) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, image->texture->native_ptr, 0);
        return true;
    } else {
        return false;
    }
}
