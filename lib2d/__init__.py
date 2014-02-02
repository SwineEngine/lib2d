"""
lib2d is a fast 2d sprite library for C and Python.
"""

__credits__ = (
"""
Copyright (C) 2013  Joseph Marshall

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
""")

__author__ = "Joseph Marshall <marshallpenguin@gmail.com>"
__version__ = "0.0.1"

_lib = _scene = None

import ctypes

class flags:
    ANIM_REPEAT = 1<<0
    ANIM_REVERSE = 1<<1
    ANIM_EXTRAPOLATE = 1<<2
    ANIM_EASE_IN = 1<<3
    ANIM_EASE_OUT = 1<<4

    ANCHOR_CENTER = 0
    ANCHOR_LEFT = 1<<10
    ANCHOR_TOP = 1<<11
    ANCHOR_RIGHT = 1<<12
    ANCHOR_BOTTOM = 1<<13

    BLEND_DISABLED = 0
    BLEND_DEFAULT = 1
    BLEND_PREMULT = 2

def init():
    global _lib, _scene
    try:
        import importlib.machinery
        f = importlib.machinery.PathFinder.find_module("_lib2d").path
    except ImportError:
        import imp
        f = imp.find_module("_lib2d")[1]
    _lib = ctypes.CDLL(f)
    _scene = _lib.l2d_scene_new(_lib.l2d_init_default_resources())

def step(dt):
    _lib.l2d_scene_step(_scene, ctypes.c_float(dt))

def render():
    _lib.l2d_scene_render(_scene)

def clear(color=0):
    _lib.l2d_scene_clear(_scene, ctypes.c_ulong(color))

def set_viewport(w, h):
    _lib.l2d_scene_set_viewport(_scene, int(w), int(h))

def set_translate(x, y, z=0, dt=0, flags=0):
    _lib.l2d_scene_set_translate(_scene,
            ctypes.c_float(x),
            ctypes.c_float(y),
            ctypes.c_float(z),
            ctypes.c_float(dt),
            int(flags))

def feed_click(x, y, button=1):
    return _lib.l2d_scene_feed_click(_scene, ctypes.c_float(x),
            ctypes.c_float(y), int(button))

class Sprite:
    # Make sure sprites are never GC'd before they are destroyed (b/c callbacks)
    __refs = set()
    def __init__(self, image="", x=0, y=0, anchor=flags.ANCHOR_CENTER):
        Sprite.__refs.add(self)
        self._anchor = anchor
        ident = _lib.l2d_ident_from_str(ctypes.c_char_p(image.encode('utf8')))
        self._ptr = _lib.l2d_sprite_new(_scene, ident, anchor)
        self._on_click = None
        self._on_anim_end = None
        self._parent = None
        if x or y:
            self.xy(x,y)

    def destroy(self, fade_out_duration=0):
        if fade_out_duration > 0.000001:
            self.a(0, fade_out_duration)
            def cb():
                _lib.l2d_sprite_delete(self._ptr)
                Sprite.__refs.remove(self)
                self._ptr = 0
            self.on_anim_end = cb
        else:
            _lib.l2d_sprite_delete(self._ptr)
            self._ptr = 0
            Sprite.__refs.remove(self)

    @property
    def parent(self):
        return self._parent
    @parent.setter
    def parent(self, p):
        assert(p is None or isinstance(p, Sprite))
        self._parent = p
        _lib.l2d_sprite_set_parent(self._ptr, p._ptr if p else None)

    def __float_wrap(self, *args):
        return [ctypes.c_float(a) for a in args]

    @property
    def on_click(self):
        return self._on_click
    @on_click.setter
    def on_click(self, cb):
        if not self._on_click and cb:
            def _cb(ud, button):
                self._on_click(button=button)
            l2d_event_cb = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p)
            self.__cbptr_onclick = l2d_event_cb(_cb)
            _lib.l2d_sprite_set_on_click(self._ptr, self.__cbptr_onclick, None)

        self._on_click = cb
        if not cb:
            _lib.l2d_sprite_set_on_click(self._ptr, None, None)
            self.__cbptr_onclick = None

    @property
    def on_anim_end(self):
        return self._on_anim_end
    @on_anim_end.setter
    def on_anim_end(self, cb):
        if not self._on_anim_end and cb:
            def _cb(*args):
                self._on_anim_end()
            l2d_sprite_cb = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)
            self.__cbptr_on_anim_end = l2d_sprite_cb(_cb)
            _lib.l2d_sprite_set_on_anim_end(self._ptr, self.__cbptr_on_anim_end, None)

        self._on_anim_end = cb
        if not cb:
            _lib.l2d_sprite_set_on_anim_end(self._ptr, None, None)
            self.__cbptr_on_anim_end = None


    def blend(self, mode=flags.BLEND_DEFAULT):
        _lib.l2d_sprite_blend(self._ptr, int(mode))

    def size(self, w, h, anchor=None):
        w = int(w)
        h = int(h)
        if anchor:
            self._anchor = anchor
        _lib.l2d_sprite_set_size(self._ptr, w, h, self._anchor)

    def order(self, order):
        _lib.l2d_sprite_set_order(self._ptr, order)

    def scale(self, scale, dt=0, flags=0):
        scale, dt = self.__float_wrap(scale, dt)
        _lib.l2d_sprite_scale(self._ptr, scale, dt, flags)

    def wrap_xy(self, x_low, x_high, y_low, y_high):
        x_low, x_high, y_low, y_high = self.__float_wrap(x_low, x_high, y_low, y_high)
        _lib.l2d_sprite_wrap_xy(self._ptr, x_low, x_high, y_low, y_high)

    def xy(self, x, y, dt=0, flags=0):
        x, y, dt = self.__float_wrap(x, y, dt)
        _lib.l2d_sprite_xy(self._ptr, x, y, dt, flags)

    def rot(self, rot, dt=0, flags=0):
        rot, dt = self.__float_wrap(rot, dt)
        _lib.l2d_sprite_rot(self._ptr, rot, dt, flags)

    def a(self, a, dt=0, flags=0):
        a, dt = self.__float_wrap(a, dt)
        _lib.l2d_sprite_a(self._ptr, a, dt, flags)

    def rgb(self, r, g, b, dt=0, flags=0):
        r, g, b, dt = self.__float_wrap(r, g, b, dt)
        _lib.l2d_sprite_rgb(self._ptr, r, g, b, dt, flags)

    def rgba(self, r, g, b, a, dt=0, flags=0):
        r, g, b, a, dt = self.__float_wrap(r, g, b, a, dt)
        _lib.l2d_sprite_rgba(self._ptr, r, g, b, a, dt, flags)

    def abort_anim(self):
        _lib.l2d_sprite_abort_anim(self._ptr)

__all__ = ['init', 'step', 'render', 'set_viewport', 'set_translate', 'clear', 'Sprite', 'flags']
