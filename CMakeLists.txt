cmake_minimum_required(VERSION 3.20)
project(probe_opengl C)

set(CMAKE_C_STANDARD 23)

set( srcs
    src/main.c
    src/stb_image.c
    src/gl.c
    src/shader_manager.c
)
include_directories(inc)

set(SDL3_VERSION 3.0)
find_package(SDL3 ${SDL3_VERSION} REQUIRED)

find_package(GLEW REQUIRED)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Werror -Wno-deprecated-declarations)
endif()

add_executable(main ${srcs})
target_link_libraries(main OpenGL  GLEW::GLEW SDL3::SDL3 m)

