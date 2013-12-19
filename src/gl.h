#ifdef GLES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include "gl_core_3_3.h"
#endif


#include <stdio.h>

#define GLDEBUG(x) \
    x; \
    { \
        GLenum e; \
        if ((e=glGetError()) != GL_NO_ERROR) \
            printf("glError 0x%x at %s line %d\n", e, __FILE__, __LINE__); \
    }

