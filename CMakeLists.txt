cmake_minimum_required(VERSION 2.8.7)
project(lib2d)

include_directories(include)

add_definitions(-std=c99 -Wall -pedantic)

set(SOURCES
    src/anim
    src/atlas
    src/atlas_bank
    src/image_bank
    src/scene
    src/sprite
    src/ident
    src/renderer
    src/matrix
    src/quaternion
    src/site
    src/resources
    src/nine_patch
    src/effect
    src/template
)

if(EMSCRIPTEN)
else()
    set(SOURCES src/gl_core_3_3 ${SOURCES})
endif()


if(EXTERNAL_RENDER)
    set(SOURCES src/renderer_external ${SOURCES})
else()
    set(SOURCES src/renderer_gl ${SOURCES})
endif()

add_library(lib2d SHARED ${SOURCES})

target_link_libraries(lib2d GL m)

install(TARGETS lib2d DESTINATION lib)
install(FILES
    include/lib2d.h
    DESTINATION include)
