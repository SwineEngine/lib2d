#ifndef ATLAS_H
#define ATLAS_H

#include <stdint.h>
#include <stdbool.h>

struct atlas;
struct atlas_entry;

/**
 * Creates a new atlas. Add entries with `atlas_add_entry` and pack them into a
 * single image with `atlas_pack`.
 */
struct atlas*
atlas_new(unsigned int bytes_per_pixel);


/**
 * Will free all atlas_entry pointers returned from `atlas_add_entry`
 */
void
atlas_delete(struct atlas*);

/**
 * If passed to atlas_add_entry flags, the l2d_sprite will be expanded 1px copying
 * the edge pixels. This is useful for avoiding pixel bleeding when using a
 * linear sampler during rasterization.
 */
static const uint32_t ATLAS_ENTRY_EXTRUDE_BORDER = 1;
/**
 * If passed to atlas_add_entry flags, the l2d_sprite will be expanded 1px with
 * transparent pixels.
 */
static const uint32_t ATLAS_ENTRY_TRANSPARENT_BORDER = 1<<1;

/**
 * Copies (atlas->bpp * width * height) bytes from data into a new atlas entry.
 * The returned atlas entry is owned by the atlas (NOT the caller.)
 * atlas_entry pointers are valid for the life of the atlas or until they are
 * passed to `atlas_remove_entry`.
 */
struct atlas_entry*
atlas_add_entry(struct atlas*, unsigned int width, unsigned int height,
        uint8_t* data, uint32_t flags);

/**
 * Removes an entry from the atlas. This will free the atlas_entry pointer.
 */
void
atlas_remove_entry(struct atlas*, struct atlas_entry*);

/**
 * Moves an entry from the `src` atlas to `dst`. This does not invalidate the
 * entry pointer, although it does transfer ownership to the `dst` atlas.
 */
void
atlas_move_entry(struct atlas* dst, struct atlas* src, struct atlas_entry*);

/**
 * Packs entries into a single image no larger than max width and height.
 * The returned pointer to the image data is owned by the caller. w and h
 * are populated with the actual width and height of the packed image.
 */
uint8_t*
atlas_pack(struct atlas*,
        unsigned int max_width, unsigned int max_height,
        unsigned int* w, unsigned int* h);

/**
 * Entries that didn't fit when the last atlas_pack was called can be queried.
 * The returned array is invalid when atlas_pack is called again.
 *
 * The array will not be modified even if an entry is removed from the atlas.
 * This accommodates looping through the array to move them to another atlas
 * that may have room (see `atlas_move_entry`).
 */
struct atlas_entry* const*
atlas_get_pack_failed(struct atlas*, unsigned int* count);

/**
 * Provides the location of an entry within the last generated packed image.
 * Values are invalid after subsequent calls to `atlas_pack`.
 */
void
atlas_entry_get_packed_location(struct atlas_entry*,
        unsigned int* x, unsigned int* y,
        unsigned int* w, unsigned int* h);


#endif

