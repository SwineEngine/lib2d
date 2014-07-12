"""
Special thanks to www.kenney.nl for the art assets used in this demo.
"""

import lib2d
from demo import demo

class Demo:
    SEQUENCES = ("climb",2), ("swim",2), ("walk",2), ("duck",1)
    def setup(self, scene):
        s = scene.make_sprite("anim/alienGreen.png")
        s.xy(320, 240)
        self.s = s

        for n,c in self.SEQUENCES:
            seq = s.new_sequence(n)
            for i in range(1,c+1):
                seq.add_frame("anim/alienGreen_"+n+str(i)+".png", 0.27)

        s.sequences['climb'].play(flags=lib2d.flags.ANIM_REPEAT)
        self.current = 0

    def on_click(self, x,y):
        self.current = (self.current+1)%len(self.SEQUENCES)
        n = self.SEQUENCES[self.current][0]
        self.s.sequences[n].play(flags=lib2d.flags.ANIM_REPEAT)

d = Demo()

demo(b"Sprite sequence demo", d.setup, d.on_click)
