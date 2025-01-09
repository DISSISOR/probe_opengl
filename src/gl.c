#include "gl.h"

Mesh *gl_meshes = nullptr;
u32 gl_meshes_size = 0;
u32 gl_meshes_cap = 0;

GLuint *gl_vaos = nullptr;

GLuint *gl_vbos_ebos;

void gl_init(u32 max_mesh_cnt, Mesh meshes_buf[static max_mesh_cnt], GLuint vaos_ebos_buf[static max_mesh_cnt * 2], GLuint vbo_sets_buf[static max_mesh_cnt]) {
    glEnable(GL_DEPTH_TEST);
    gl_meshes = meshes_buf;
    gl_meshes_size = 0;
    gl_meshes_cap = max_mesh_cnt;
    gl_vaos = vaos_ebos_buf;
    gl_vbos_ebos = vbo_sets_buf;
}

void gl_mesh_init(u32 n, MeshHandle handle_buf[static n], Vertex *verts[static n], GLuint *indices[static n], u32 vert_cnts[static n], u32 indices_cnt[static n]) {
    MY_ASSERT(gl_meshes && gl_vaos && gl_vbos_ebos);
    GLint prev_vao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    glGenVertexArrays(n, gl_vaos+gl_meshes_size);
    glGenBuffers(n * 2, gl_vbos_ebos + gl_meshes_size * 2);
    for (u32 i=0; i<n; i++) {
        handle_buf[i].index = gl_meshes_size + i;
        const MeshHandle handle = handle_buf[i];
        Mesh *const mesh = gl_mesh_get_data(handle);
        const GLuint *const vao = gl_mesh_get_vao(handle);
        const GLuint *const vbo = gl_mesh_get_vbo(handle);
        const GLuint *const ebo = gl_mesh_get_ebo(handle);
        mesh->verts = verts[i];
        mesh->indices = indices[i];
        mesh->indices_cnt = indices_cnt[i];
        glBindVertexArray(*vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indices_cnt[i], indices[i], GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, *vbo);
        glBufferData(GL_ARRAY_BUFFER, vert_cnts[i] * sizeof(Vertex), verts[i], GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, coord)));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, texcoord)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal)));
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, color)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
    }
    gl_meshes_size += n;
    glBindVertexArray(prev_vao);
}

GLuint* gl_mesh_get_vao(MeshHandle handle) {
    MY_ASSERT(gl_vaos);
    return &gl_vaos[handle.index];
}

Mesh* gl_mesh_get_data(MeshHandle handle) {
    MY_ASSERT(gl_vaos);
    return &gl_meshes[handle.index];
}

GLuint* gl_mesh_get_vbo(MeshHandle handle) {
    MY_ASSERT(gl_vbos_ebos);
    return &gl_vbos_ebos[handle.index * 2];
}

GLuint* gl_mesh_get_ebo(MeshHandle handle) {
    MY_ASSERT(gl_vbos_ebos);
    return &gl_vbos_ebos[handle.index * 2 + 1];
}

void gl_mesh_draw(MeshHandle handle) {
    const Mesh *const mesh = gl_mesh_get_data(handle);
    glBindVertexArray(gl_vaos[handle.index]);
    glDrawElements(GL_TRIANGLES, mesh->indices_cnt, GL_UNSIGNED_INT, 0);
}
