#ifndef SHADER_MANAGER_H_
#define SHADER_MANAGER_H_

#include <fcntl.h>

#include <GL/glew.h>

#include "common.h"

typedef struct ShaderInfo {
    StringView source_file_path;
    int source_file_fd;
    int watch_fd;
    union {
        GLuint shader;
        GLint64 shader_opt;
    };
} ShaderInfo;

bool shader_info_init_and_compile(ShaderInfo *res, StringView path, GLenum shaderype);

typedef struct ShaderMgr {
    ShaderInfo vertex;
    ShaderInfo fragment;
    int inotify_fd;
    GLuint prog;
    bool have_prog;
} ShaderMgr;

typedef enum ShaderMgrError {
    SHADER_MGR_ERROR_NONE = 0,
    SHADER_MGR_ERROR_SHADER_FD_OPEN,
    SHADER_MGR_ERROR_INOTIFY_INIT,
    SHADER_MGR_ERROR_ADD_WATCH,
    SHADER_MGR_ERROR_COMPILE_VERT_SHADER,
    SHADER_MGR_ERROR_COMPILE_FRAG_SHADER,
    SHADER_MGR_ERROR_LINK_PROGRAM,
} ShaderMgrError;

ShaderMgrError shader_mgr_reload_if_needed(ShaderMgr *mgr, bool *reloaded);
ShaderMgrError shader_mgr_reload_shaders(ShaderMgr *mgr, u32 buf_size, u8 buf[static buf_size]);
ShaderMgrError shader_mgr_get_program(ShaderMgr *mgr, GLuint *prog, u32 buf_size, u8 buf[static buf_size]);
ShaderMgrError shader_mgr_init(ShaderMgr *mgr, StringView vertex_path, StringView fragment_path, u32 buf_size, u8[static buf_size]);

#endif // SHADER_MANAGER_H_
