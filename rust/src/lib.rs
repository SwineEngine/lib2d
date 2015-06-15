#![allow(dead_code)]
extern crate libc;
use libc::{c_int, c_float, c_char, c_void};
use std::ffi::CStr;
use std::str::FromStr;

#[repr(C)]
struct l2d_image;

#[repr(C)]
struct l2d_resources;

#[repr(C)]
struct l2d_scene;

#[repr(C)]
struct l2d_sprite;

#[repr(C)]
struct l2d_anim;

#[repr(C)]
struct l2d_effect;

#[repr(C)]
type l2d_ident = u64;

#[derive(Clone, Copy)]
#[repr(C)]
pub enum l2d_blend {
    l2d_BLEND_DISABLED,
    l2d_BLEND_DEFAULT,
    l2d_BLEND_PREMULT,
}

#[derive(Clone, Copy)]
#[repr(C)]
pub enum l2d_image_format {
    l2d_IMAGE_FORMAT_RGBA_8888,
    l2d_IMAGE_FORMAT_RGB_888,
    l2d_IMAGE_FORMAT_RGB_565,
    l2d_IMAGE_FORMAT_A_8,
}

#[link(name="lib2d")]
extern {
    fn l2d_clear(color: u32);
    fn l2d_init_default_resources() -> *const l2d_resources;

    fn l2d_ident_from_str(str: *const c_char) -> l2d_ident;
    fn l2d_ident_from_strn(str: *const c_char, len: c_int) -> l2d_ident;
    fn l2d_ident_as_char(ident: l2d_ident) -> *const c_char;

    fn l2d_anim_new(anim_list: *mut *const l2d_anim, to_v: c_float, dt: c_float, flags: u32);
    fn l2d_anim_step(anim_list: *mut *const l2d_anim, dt: c_float, dst: *mut c_float) -> bool;
    fn l2d_anim_release_all(anim_list: *mut *const l2d_anim);

    fn l2d_scene_new(resources: *const l2d_resources) -> *const l2d_scene;
    fn l2d_scene_delete(scene: *const l2d_scene);
    fn l2d_scene_get_resources(scene: *const l2d_scene) -> *const l2d_resources;
    fn l2d_scene_step(scene: *const l2d_scene, dt: c_float);
    fn l2d_scene_render(scene: *const l2d_scene);
    fn l2d_scene_set_viewport(scene: *const l2d_scene, w: c_int, h: c_int);
    fn l2d_scene_set_translate(scene: *const l2d_scene, x: c_float, y: c_float, z: c_float, dt: c_float, flags: u32);
    fn l2d_scene_feed_click(scene: *const l2d_scene, x: c_float, y: c_float, button: c_int);

    fn l2d_sprite_new(scene: *const l2d_scene, image: l2d_ident, flags: u32) -> *const l2d_sprite;
    fn l2d_sprite_delete(sprite: *const l2d_sprite);
    fn l2d_sprite_get_scene(sprite: *const l2d_sprite) -> *const l2d_scene;
    fn l2d_sprite_set_image(sprite: *const l2d_sprite, image: *const l2d_image);
    fn l2d_sprite_get_image_width(sprite: *const l2d_sprite) -> c_int;
    fn l2d_sprite_get_image_height(sprite: *const l2d_sprite) -> c_int;
    fn l2d_sprite_set_parent(sprite: *const l2d_sprite, parent: *const l2d_sprite);
    fn l2d_sprite_set_size(sprite: *const l2d_sprite, w: c_int, h: c_int, sprite_flags: u32);
    fn l2d_sprite_set_order(sprite: *const l2d_sprite, order: c_int);
    //fn l2d_sprite_set_on_click(sprite: *const l2d_sprite, );
    //fn l2d_sprite_set_on_anim_end(sprite: *const l2d_sprite, );
    fn l2d_sprite_set_stop_anims_on_hide(sprite: *const l2d_sprite, v: bool);
    fn l2d_sprite_set_effect(sprite: *const l2d_sprite, e: *const l2d_effect);
    fn l2d_sprite_feed_click(sprite: *const l2d_sprite, x: c_float, y: c_float, button: c_int) -> bool;
    fn l2d_sprite_blend(sprite: *const l2d_sprite, mode: l2d_blend);
    fn l2d_sprite_xy(sprite: *const l2d_sprite, x: c_float, y: c_float, dt: c_float, anim_flags: u32);
    fn l2d_sprite_wrap_xy(sprite: *const l2d_sprite, x_low: c_float, x_high: c_float, y_low: c_float, y_high: c_float);
    fn l2d_sprite_scale(sprite: *const l2d_sprite, scale: c_float, dt: c_float, anim_flags: u32);
    fn l2d_sprite_rot(sprite: *const l2d_sprite, rot: c_float, dt: c_float, anim_flags: u32);
    fn l2d_sprite_a(sprite: *const l2d_sprite, a: c_float, dt: c_float, anim_flags: u32);
    fn l2d_sprite_rgb(sprite: *const l2d_sprite, r: c_float, g: c_float, b: c_float, dt: c_float, anim_flags: u32);
    fn l2d_sprite_rgba(sprite: *const l2d_sprite, r: c_float, g: c_float, b: c_float, a: c_float, dt: c_float, anim_flags: u32);
    fn l2d_sprite_step(sprite: *const l2d_sprite, dt: c_float);
    fn l2d_sprite_abort_anim(sprite: *const l2d_sprite);
    fn l2d_sprite_new_sequence(sprite: *const l2d_sprite) -> c_int;
    fn l2d_sprite_sequence_add_frame(sprite: *const l2d_sprite, sequence: c_int, image: l2d_ident, duration: c_float, image_flags: u32);
    fn l2d_sprite_sequence_play(sprite: *const l2d_sprite, sequence: c_int, start_frame: c_int, speed_multiplier: c_float, anim_flags: u32);
    fn l2d_sprite_sequence_stop(sprite: *const l2d_sprite);

    fn l2d_resources_load_image(resources: *const l2d_resources, image: l2d_ident, flags: u32) -> *const l2d_image;
    fn l2d_image_bind(image: *const l2d_image, handle: i32, texture_slot: c_int);
    fn l2d_set_image_data(scene: *const l2d_scene, key: l2d_ident, width: c_int, height: c_int, format: l2d_image_format, data: *const c_void, flags: u32);
    fn l2d_image_new(scene: *const l2d_scene, width: c_int, height: c_int, format: l2d_image_format, data: *const c_void, flags: u32) -> *const l2d_image;
    fn l2d_image_release(image: *const l2d_image);
}

pub struct Resources {
    raw: *const l2d_resources,
}

impl Resources {
    pub fn init_default() -> Resources {
        unsafe {
            Resources {
                raw: l2d_init_default_resources(),
            }
        }
    }
}

pub const SPRITE_ANCHOR_LEFT:u32 = 1<<10;
pub const SPRITE_ANCHOR_TOP:u32 = 1<<11;
pub const SPRITE_ANCHOR_RIGHT:u32 = 1<<12;
pub const SPRITE_ANCHOR_BOTTOM:u32 = 1<<13;

pub const ANIM_REPEAT:u32 = 1<<0;
pub const ANIM_REVERSE:u32 = 1<<1;
pub const ANIM_EXTRAPOLATE:u32 = 1<<2;
pub const ANIM_EASE_IN:u32 = 1<<3;
pub const ANIM_EASE_OUT:u32 = 1<<4;


pub struct Sprite {
    raw: *const l2d_sprite,
}

impl Drop for Sprite {
    fn drop(&mut self) {
        unsafe {
            l2d_sprite_delete(self.raw);
        }
    }
}

impl Sprite {
    pub fn new(scene: &mut Scene, image: Ident, flags: u32) -> Sprite {
        unsafe {
            Sprite {
                raw: l2d_sprite_new(scene.raw, image.as_u64(), flags),
            }
        }
    }

    pub fn set_size(&mut self, w: usize, h: usize, sprite_flags: u32) {
        unsafe {
            l2d_sprite_set_size(self.raw, w as c_int, h as c_int, sprite_flags);
        }
    }

    pub fn xy(&mut self, x: f32, y: f32, dt: f32, anim_flags: u32) {
        unsafe {
            l2d_sprite_xy(self.raw, x as c_float, y as c_float, dt as c_float, anim_flags);
        }
    }

    pub fn a(&mut self, a: f32, dt: f32, anim_flags: u32) {
        unsafe {
            l2d_sprite_a(self.raw, a as c_float, dt as c_float, anim_flags);
        }
    }

    pub fn rgb(&mut self, r: f32, g: f32, b: f32, dt: f32, anim_flags: u32) {
        unsafe {
            l2d_sprite_rgb(self.raw, r as c_float, g as c_float, b as c_float, dt as c_float, anim_flags);
        }
    }

    pub fn rgba(&mut self, r: f32, g: f32, b: f32, a: f32, dt: f32, anim_flags: u32) {
        unsafe {
            l2d_sprite_rgba(self.raw, r as c_float, g as c_float, b as c_float, a as c_float, dt as c_float, anim_flags);
        }
    }
}


pub struct Scene {
    raw: *const l2d_scene,
}

impl Drop for Scene {
    fn drop(&mut self) {
        unsafe {
            l2d_scene_delete(self.raw);
        }
    }
}

impl Scene {
    pub fn new(resources: Resources) -> Scene {
        unsafe {
            Scene {
                raw: l2d_scene_new(resources.raw),
            }
        }
    }

    pub fn get_resources(&mut self) -> Resources {
        unsafe {
            Resources { raw: l2d_scene_get_resources(self.raw) }
        }
    }

    pub fn step(&mut self, dt: f32) {
        unsafe {
            l2d_scene_step(self.raw, dt as c_float)
        }
    }

    pub fn render(&mut self) {
        unsafe {
            l2d_scene_render(self.raw)
        }
    }
}


#[derive(PartialEq, PartialOrd, Copy, Clone, Ord, Eq, Hash)]
pub struct Ident(pub l2d_ident);

impl Ident {
    pub fn from_str(string: &str) -> Ident {
        unsafe {
            Ident(l2d_ident_from_strn(string.as_ptr() as *const c_char, string.len() as i32))
        }
    }
    pub fn as_u64(self) -> u64 {
        let Ident(u) = self;
        u
    }
    pub fn as_str(self) -> &'static str {
        unsafe {
            let p = l2d_ident_as_char(self.as_u64());
            if !p.is_null() {
                std::str::from_utf8(CStr::from_ptr(p).to_bytes()).unwrap()
            } else {
                ""
            }
        }
    }
}

pub struct InvalidIdentError;
impl FromStr for Ident {
    type Err = InvalidIdentError;
    fn from_str(s: &str) -> Result<Ident, InvalidIdentError> {
        Ok(Ident::from_str(s))
    }
}
