#include "shader_manager.h"

#include <string.h>

#include <limits.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#include "common.h"
#include "arena.h"

static i64 read_whole_file(int fd, Arena *arena, StringView *content) {
    struct stat stat;
    fstat(fd, &stat);
    content->size = stat.st_size + 1;
    content->data = ARENA_MAKE(arena, char, content->size);
    int n = read(fd, content->data, content->size);
    if (n < 0) return -1;
    content->data[content->size-1] = 0;
    return content->size + 1;
}

GLint64 compile_shader_from_file(int fd, GLenum shader_type, Arena *arena, StringView *log) {
    lseek(fd, 0, SEEK_SET);
    StringView shader_source;
    i64 read = read_whole_file(fd, arena, &shader_source);
    if (read < 0) {
        return -1;
    }
    const char* source[] = { (const char*)shader_source.data };
    const GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        log->data = ARENA_MAKE(arena, char, log_length+1);
        log->size = log_length + 1;
        glGetShaderInfoLog(shader, log_length, NULL, (GLchar*)log->data);
        log->data[log_length] = 0;
        return -1;
    }
    return shader;
}

static ShaderMgrError shader_info_init(ShaderInfo *shader, StringView path, GLenum type, Arena *arena, StringView *log) {
    shader->source_file_fd = open(path.data, O_RDONLY);
    shader->source_file_path = path;
    if (shader->source_file_fd < 0) return SHADER_MGR_ERROR_SHADER_FD_OPEN;
    shader->shader_opt = compile_shader_from_file(shader->source_file_fd, type, arena, log);
    if (shader->shader_opt < 0) {
        return type == GL_VERTEX_SHADER ? SHADER_MGR_ERROR_COMPILE_VERT_SHADER : SHADER_MGR_ERROR_COMPILE_FRAG_SHADER;
    }
    return SHADER_MGR_ERROR_NONE;
}

static ShaderMgrError shader_mgr_init_impl(ShaderMgr *mgr, ShaderInfo vertex, ShaderInfo fragment) {
    mgr->inotify_fd = inotify_init();
    if (mgr->inotify_fd < 0) {
        return SHADER_MGR_ERROR_INOTIFY_INIT;
    }
    vertex.watch_fd = inotify_add_watch(mgr->inotify_fd, vertex.source_file_path.data, IN_MODIFY);
    if (vertex.watch_fd < 0) {
        return SHADER_MGR_ERROR_ADD_WATCH;
    }
    fragment.watch_fd = inotify_add_watch(mgr->inotify_fd, fragment.source_file_path.data, IN_MODIFY);
    if (fragment.watch_fd < 0) {
        return SHADER_MGR_ERROR_ADD_WATCH;
    }
    int flags = fcntl(mgr->inotify_fd, F_GETFL, 0);
    fcntl(mgr->inotify_fd, F_SETFL, flags | O_NONBLOCK);
    mgr->vertex = vertex;
    mgr->fragment = fragment;
    mgr->have_prog = false;
    return SHADER_MGR_ERROR_NONE;
}

ShaderMgrError shader_mgr_init(ShaderMgr *mgr, StringView vertex_path, StringView fragment_path, Arena *arena, StringView *log) {
    MY_ASSERT(memchr(vertex_path.data, 0, vertex_path.size));
    MY_ASSERT(memchr(fragment_path.data, 0, fragment_path.size));
    ShaderInfo vertex;
    ShaderInfo fragment;
    ShaderMgrError err;
    err = shader_info_init(&vertex, vertex_path, GL_VERTEX_SHADER, arena, log);
    if (err != SHADER_MGR_ERROR_NONE) {
        return err;
    }
    err = shader_info_init(&fragment, fragment_path, GL_FRAGMENT_SHADER, arena, log);
    if (err != SHADER_MGR_ERROR_NONE) {
        return err;
    }
    return shader_mgr_init_impl(mgr, vertex, fragment);
}

ShaderMgrError shader_mgr_get_program(ShaderMgr *mgr, GLuint *prog, Arena *arena, StringView *log) {
    if (mgr->have_prog) {
        *prog = mgr->prog;
        return SHADER_MGR_ERROR_NONE;
    };
    mgr->prog = glCreateProgram();
    glAttachShader(mgr->prog, mgr->vertex.shader);
    glAttachShader(mgr->prog, mgr->fragment.shader);
    glLinkProgram(mgr->prog);

    GLint success;
    glGetProgramiv(mgr->prog, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_length;
        glGetProgramiv(mgr->prog, GL_INFO_LOG_LENGTH, &log_length);
        log->size = log_length + 1;
        log->data = ARENA_MAKE(arena, char, log_length+1);
        log->data[log_length] = 0;
        glGetProgramInfoLog(mgr->prog, log_length, NULL, log->data);
        return SHADER_MGR_ERROR_LINK_PROGRAM;
    }
    *prog = mgr->prog;
    mgr->have_prog = prog;
    return SHADER_MGR_ERROR_NONE;
}

ShaderMgrError shader_mgr_reload_if_needed(ShaderMgr *mgr, bool *reloaded, Arena *arena, StringView *log) {
    enum { BUF_SIZE = sizeof(struct inotify_event) + NAME_MAX + 1  };
    u8 buf[BUF_SIZE];
    *reloaded = false;
    for (;;) {
        auto len = read(mgr->inotify_fd, buf, sizeof(buf));
        if (len <= 0) break;
        const struct inotify_event *event;
        // FIXME: reloads EVERY shader for EVERY inotify event
        for (u8 *ptr = buf; ptr < buf + len;
            ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event*)(buf);
            ShaderMgrError err = shader_mgr_reload_shaders(mgr, arena, log);
            if (err != 0) {
                return err;
            }
            *reloaded = true;
        }
    }
    return 0;
}

ShaderMgrError shader_mgr_reload_shaders(ShaderMgr *mgr, Arena *arena, StringView *log) {
    MY_ASSERT(mgr->have_prog);
    GLint64 tmp_vert = compile_shader_from_file(mgr->vertex.source_file_fd, GL_VERTEX_SHADER, arena, log);
    GLint64 tmp_frag = compile_shader_from_file(mgr->fragment.source_file_fd, GL_FRAGMENT_SHADER, arena, log);
    if (tmp_vert < 0) {
        return SHADER_MGR_ERROR_COMPILE_VERT_SHADER;
    }
    if (tmp_frag < 0) {
        return SHADER_MGR_ERROR_COMPILE_FRAG_SHADER;
    }
    GLint prog_tmp = glCreateProgram();
    glAttachShader(prog_tmp, tmp_vert);
    glAttachShader(prog_tmp, tmp_frag);
    glLinkProgram(prog_tmp);
    GLint success;
    glGetProgramiv(mgr->prog, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_length;
        glGetProgramiv(mgr->prog, GL_INFO_LOG_LENGTH, &log_length);
        log->size = log_length + 1;
        log->data = ARENA_MAKE(arena, char, log->size);
        log->data[log_length] = 0;
        glGetProgramInfoLog(mgr->prog, log_length, NULL, (char*)log->data);
        return SHADER_MGR_ERROR_LINK_PROGRAM;
    }

    glDeleteShader(mgr->vertex.shader);
    glDeleteShader(mgr->fragment.shader);
    glUseProgram(prog_tmp);
    glDeleteProgram(mgr->prog);
    mgr->prog = prog_tmp;

    mgr->vertex.shader = tmp_vert;
    mgr->fragment.shader = tmp_frag;
    return 0;
}

