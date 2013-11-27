import random

import lib2d
from sdl2 import *
import ctypes

SDL_Init(SDL_INIT_VIDEO)
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

win = SDL_CreateWindow(b"lib2d lotsofsprites demo",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE)
ctx = SDL_GL_CreateContext(win)
SDL_GL_MakeCurrent(win, ctx)
SDL_GL_SetSwapInterval(0)

lib2d.init()


r = lambda: random.random()

for i in range(2400):
    s = lib2d.Sprite("rounded_square.png")
    s.blend(lib2d.flags.BLEND_PREMULT)
    s.rgba(.5, .2, 1, .2)
    s.rgba(0, .8, 0, .6, 3*r()+2, lib2d.flags.ANIM_REVERSE)

    s.xy(r()*640, r()*480)
    s.xy(r()*640, r()*480, 2, lib2d.flags.ANIM_EXTRAPOLATE)
    s.wrap_xy(0,640, 0,480)

    s.scale(.1)
    s.scale(1, r()+.75, lib2d.flags.ANIM_REVERSE)

    s.rot(360, 2, lib2d.flags.ANIM_REPEAT)

print("Drawing 2,400 sprites...")

running = True
event = SDL_Event()

next_print = 1000
frame_start = 0;
frames_past = 0
while running:
    now = SDL_GetTicks()
    dt = now-frame_start
    frame_start = SDL_GetTicks();
    next_print -= dt
    frames_past += 1
    if dt < 1:
        SDL_Delay(1)
        dt = 1
    if next_print <= 0:
        SDL_SetWindowTitle(win, (str(frames_past)+" FPS").encode('utf8'))
        frames_past = 0
        next_print = 1000


    while SDL_PollEvent(ctypes.byref(event)) != 0:
        if event.type == SDL_QUIT:
            running = False
            break
        elif event.type == SDL_WINDOWEVENT:
            if event.window.event == SDL_WINDOWEVENT_RESIZED:
                lib2d.set_viewport(event.window.data1, event.window.data2)

    lib2d.step(dt*0.001)
    lib2d.clear()
    lib2d.render()
    SDL_GL_SwapWindow(win);
    
SDL_DestroyWindow(win)
SDL_Quit()
