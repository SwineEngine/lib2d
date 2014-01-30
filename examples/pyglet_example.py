import lib2d
import pyglet

window = pyglet.window.Window(width=640, height=480, caption="Minimal lib2d example using pyglet")
lib2d.init()

s = lib2d.Sprite("rounded_square.png")
s.blend(lib2d.flags.BLEND_PREMULT)
s.rgb(.5, .2, 1)
s.xy(320, 240)
s.rot(360, 2, lib2d.flags.ANIM_REPEAT)

@window.event
def on_draw():
    window.clear()
    lib2d.render()

while not window.has_exit:
    lib2d.step(pyglet.clock.tick())
    window.dispatch_events()
    window.dispatch_event('on_draw')
    window.flip()
window.close()
