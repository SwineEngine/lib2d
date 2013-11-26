#include <lib2d.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_events.h>

static
float
r() {
    return rand()/(float)RAND_MAX;
}

int
main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_Window* win = SDL_CreateWindow("lib2d lotsofsprites demo",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
            SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, ctx);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetSwapInterval(0);

    struct l2d_scene* scene = l2d_scene_new(l2d_init_default_resources());

    for (int i=0; i<2400; i++) {
        struct l2d_sprite* s = l2d_sprite_new(scene,
                l2d_ident_from_str("rounded_square.png"), 0);
        l2d_sprite_rgba(s, .5, .2, 1, .2,  0, 0);
        l2d_sprite_rgba(s, 0, .8, 0, .6, 3*r()+2, l2d_ANIM_REVERSE);

        l2d_sprite_xy(s, r()*640, r()*480,  0, 0);
        l2d_sprite_xy(s, r()*640, r()*480, 2, l2d_ANIM_EXTRAPOLATE);
        l2d_sprite_wrap_xy(s, 0, 640, 0, 480);

        l2d_sprite_scale(s, .1,  0, 0);
        l2d_sprite_scale(s, 1, r()+.75f, l2d_ANIM_REVERSE);

        l2d_sprite_rot(s, 360, 2, l2d_ANIM_REPEAT);
    }

    int next_print = 1000;
    int running = 1;
    uint32_t frame_start = 0;
    int frames_past = 0;
    while (running) {
        uint32_t now = SDL_GetTicks();
        int dt = now-frame_start;
        frame_start = SDL_GetTicks();
        next_print -= dt;
        frames_past += 1;
        if (dt < 1) {
            SDL_Delay(1);
            dt = 1;
        }

        if (next_print <= 0) {
            char title[32];
            sprintf(title, "%d FPS", frames_past);
            SDL_SetWindowTitle(win, title);
            frames_past = 0;
            next_print = 1000;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                        l2d_scene_set_viewport(scene, e.window.data1, e.window.data2);
                    }
                    break;
            }
        }

        l2d_scene_step(scene, dt * .001f);
        l2d_scene_clear(scene, 0x0);
        l2d_scene_render(scene);
        SDL_GL_SwapWindow(win);
    }

    return 0;
}
