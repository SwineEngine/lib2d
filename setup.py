from distutils.core import setup, Extension
import os, re

native = Extension('_lib2d',
                    define_macros = [],
                    include_dirs = ['include'],
                    libraries = ['GL', 'm'],
                    library_dirs = [],
                    extra_compile_args=['-std=c99','-O3'],
                    sources = ['src/'+f for f in os.listdir('src') if
                        f.endswith('.c') and f != "stb_image.c"],
                    language='c')

version = re.search(r'__version__ = "([0-9\.a-z\-]+)"',
        open("lib2d/__init__.py").read()).groups()[0]

setup(
    name = 'lib2d',
    version = version,
    author = "Joseph Marshall",
    author_email = "marshallpenguin@gmail.com",
    description = "A 2D drawing an animation engine using OpenGL",
    license = "MIT",
    url="http://arcticpaint.com/lib2d/",
    long_description=open("README").read(),

    packages = ["lib2d"],
    ext_modules=[native],

    classifiers = [
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: C',
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Topic :: Multimedia :: Graphics',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],

    scripts=['scripts/premultiplyalpha.py']
)
