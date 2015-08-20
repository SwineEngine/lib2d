cmake_minimum_required(VERSION 2.8.7)
project(lib2d)

include_directories(include)

add_definitions(-std=c99 -Wall -pedantic -fvisibility=hidden)

set(SOURCES
    src/anim
    src/atlas
    src/atlas_bank
    src/image_bank
    src/scene
    src/siphash
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
    src/target
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

if (IOS)
    macro(ADD_FRAMEWORK fwname appname)
        find_library(FRAMEWORK_${fwname}
            NAMES ${fwname}
            PATHS ${CMAKE_IOS_SDK_ROOT}/System/Library
            PATH_SUFFIXES Frameworks
            NO_DEFAULT_PATH)
        if( ${FRAMEWORK_${fwname}} STREQUAL FRAMEWORK_${fwname}-NOTFOUND)
            MESSAGE(ERROR ": Framework ${fwname} not found")
        else()
            TARGET_LINK_LIBRARIES(${appname} ${FRAMEWORK_${fwname}})
            MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
        endif()
    endmacro(ADD_FRAMEWORK)
    ADD_FRAMEWORK(OpenGLES lib2d m)
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(lib2d ${OPENGL_LIBRARIES} m)
elseif(WIN32)
    target_link_libraries(lib2d opengl32)
elseif(BROADCOM_LINUX)
    target_link_libraries(lib2d khronos m)
else()
    target_link_libraries(lib2d GL m)
endif()


install(TARGETS lib2d DESTINATION lib)
install(FILES
    include/lib2d.h
    DESTINATION include)
