#ifndef gl_h_INCLUDED
#define gl_h_INCLUDED

#include <GL/glew.h>

#include "common.h"

typedef struct MeshHandle {
    u32 index;
} MeshHandle;

typedef struct Vertex {
    f32 coord[3];
    f32 texcoord[2];
    f32 normal[3];
    f32 color[3];
} Vertex;

static_assert(alignof(Vertex) == 4);
static_assert(sizeof(Vertex) == 11 * sizeof(f32));

typedef struct Mesh {
    Vertex *verts;
    GLuint *indices;
    size_t indices_cnt;
} Mesh;


typedef enum GlError {
    GL_ERROR_NONE = 0,
    GL_ERROR_BUFF_SIZE_TOO_SMALL,
} GlError;

void gl_init(u32 max_mesh_cnt, Mesh meshes_buf[static max_mesh_cnt], GLuint vaos_ebos_buf[static max_mesh_cnt * 2], GLuint vbo_sets_buf[static max_mesh_cnt]);
void gl_mesh_init(u32 n, MeshHandle handle_buf[static n], Vertex *verts[static n], GLuint *indices[static n], u32 vert_cnts[static n], u32 indices_cnt[static n]);
GLuint* gl_mesh_get_vao(MeshHandle handle);
Mesh* gl_mesh_get_data(MeshHandle handle);
GLuint* gl_mesh_get_vbo(MeshHandle handle);
GLuint* gl_mesh_get_ebo(MeshHandle handle);

void gl_mesh_draw(MeshHandle handle);

#endif // gl_h_INCLUDED

