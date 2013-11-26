#!/usr/bin/python
import Image
import numpy
import sys

for filename in sys.argv[1:]:
    im = Image.open(filename).convert('RGBA')
    a = numpy.fromstring(im.tostring(), dtype=numpy.uint8)
    alphaLayer = a[3::4] / 255.0
    a[::4]  *= alphaLayer
    a[1::4] *= alphaLayer
    a[2::4] *= alphaLayer
    im = Image.fromstring("RGBA", im.size, a.tostring())
    im.save(filename, "PNG")
