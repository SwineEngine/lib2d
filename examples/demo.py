import lib2d
from sdl2 import *
import ctypes

class WinInfo:
    w = 640
    h = 480
    x = 0
    y = 0
    scene = None

    @classmethod
    def set_translate(cls, x, y):
        cls.x = x
        cls.y = y
        scene.set_translate(x,y, dt=.3, flags=lib2d.flags.ANIM_EASE_IN)

    @classmethod
    def transform(cls, x, y):
        return x-cls.x, y-cls.y


def demo(title, setup, on_click=None, framelock=True):
    SDL_Init(SDL_INIT_VIDEO)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    win = SDL_CreateWindow(title,
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            WinInfo.w, WinInfo.h,
            SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE)
    ctx = SDL_GL_CreateContext(win)
    SDL_GL_MakeCurrent(win, ctx)
    if framelock:
        SDL_GL_SetSwapInterval(1)
    else:
        SDL_GL_SetSwapInterval(0)

    lib2d.init()

    scene = lib2d.Scene()
    WinInfo.scene = scene

    setup(scene)


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
        if dt < 1:
            SDL_Delay(1)
            dt = 1
        if not framelock:
            frames_past += 1
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
                    scene.set_viewport(event.window.data1, event.window.data2)
                    WinInfo.w = event.window.data1
                    WinInfo.h = event.window.data2

            elif event.type == SDL_MOUSEBUTTONDOWN:
                r = scene.feed_click(
                        *WinInfo.transform(event.button.x, event.button.y),
                        button=event.button.button)
                if not r and on_click:
                    on_click(event.button.x, event.button.y)

        scene.step(dt*0.001)
        lib2d.clear()
        scene.render()
        SDL_GL_SwapWindow(win);
        
    SDL_DestroyWindow(win)
    SDL_Quit()
if __name__ == "__main__":
    print("This is just a template for demos. Don't run directly.")
