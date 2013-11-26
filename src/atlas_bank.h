#ifndef __atlas_bank_H__
#define __atlas_bank_H__

#include "stdint.h"
#include "stdbool.h"
#include "image_bank.h"

struct atlas_bank;
struct atlas_bank_entry;

struct atlas_bank*
atlas_bank_new(void);

void
atlas_bank_delete(struct atlas_bank* atlas_bank);

struct l2d_image_bank;
bool
atlas_bank_resolve(struct atlas_bank* atlas_bank, struct l2d_image_bank*);

struct atlas_bank_entry*
atlas_bank_new_entry(struct atlas_bank*, int width, int height,
        uint8_t* use_data, enum l2d_image_format, uint32_t flags);

struct texture*
atlas_bank_get_texture(struct atlas_bank_entry*);

struct rect
atlas_bank_get_region(struct atlas_bank_entry*);

#endif
