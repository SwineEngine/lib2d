import random
import lib2d
from demo import demo


def setup():
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


demo(b"lib2d lotsofsprites demo", setup, framelock=False)
