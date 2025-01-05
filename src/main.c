#include <stdbool.h>

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include "common.h"
#include "gl.h"
#include "shader_manager.h"

#define RAYMATH_STATIC_INLINE
#include "raymath.h"

#define EXCEPT_SUCC_SDL(val, goto_label) do {\
if (!(val)) {\
    SDL_Log("%s\n", SDL_GetError());\
    retval = -1;\
    goto goto_label;\
}} while(0)

static const bool *kb_state;
static bool prev_kb_state[512];

static bool is_key_pressed(SDL_Scancode code);
static bool is_key_just_pressed(SDL_Scancode code);

typedef struct Transform {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
} Transform;

typedef struct GameObject {
    MeshHandle mesh;
    Transform transform;
} GameObject;

typedef struct Camera {
    Vector3 up;
    Vector3 eye;
    Vector3 traget;
} Camera;

static void game_object_draw(const GameObject *obj, const Camera *cam);

static Vertex cube_verts[] = {
    { {-0.5, -0.5, -0.5}, {0, 0}, {0, 0, -1}, {1, 0, 0} }, // 0
    { {-0.5, -0.5,  0.5}, {1, 0}, {0, 0, -1}, {1, 0, 0} }, // 1
    { {-0.5,  0.5, -0.5}, {0, 1}, {0, 0, -1}, {1, 0, 0} }, // 2
    { {-0.5,  0.5,  0.5}, {1, 1}, {0, 0, -1}, {1, 0, 0} }, // 3
    { { 0.5, -0.5, -0.5}, {0, 0}, {0, 0, 1}, {0, 1, 0} }, // 4
    { { 0.5, -0.5,  0.5}, {1, 0}, {0, 0, 1}, {0, 1, 0} }, // 5
    { { 0.5,  0.5, -0.5}, {0, 1}, {0, 0, 1}, {0, 1, 0} }, // 6
    { { 0.5,  0.5,  0.5}, {1, 1}, {0, 0, 1}, {0, 1, 0} }  // 7
};
static_assert(ARRAY_LEN(cube_verts) == 8);
static GLuint cube_indices[] = {
    0, 1, 2,
    0, 2, 3,

    4, 5, 6,
    4, 6, 7,

    0, 1, 5,
    0, 5, 4,

    2, 3, 7,
    2, 7, 6,

    0, 3, 7,
    0, 7, 4,

    1, 2, 6,
    1, 6, 5
};
static_assert(ARRAY_LEN(cube_indices) == 6 * 6);

static Mesh meshes[4096];
static GLuint vaos[ARRAY_LEN(meshes)];
static GLuint vbos_ebos [ARRAY_LEN(meshes) * 2];

u8 tmp_buf[1024 * 1024 * 5];

#define VERTEX_SHADER_PATH "shaders/vert.glsl"
#define FRAGMENT_SHADER_PATH "shaders/frag.glsl"

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    int retval = 0;
    EXCEPT_SUCC_SDL(SDL_Init(SDL_INIT_VIDEO), return_lbl);
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_Window *const win = SDL_CreateWindow("Hello", 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    EXCEPT_SUCC_SDL(win, quit_sdl_lbl);
    SDL_GLContext gl_ctx = SDL_GL_CreateContext(win);
    EXCEPT_SUCC_SDL(gl_ctx, destroy_win_lbl);
    glewExperimental = true;
    const GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        retval = -1;
        SDL_Log("%s\n", glewGetErrorString(glew_err));
        goto destroy_gl_ctx_lbl;
    }

    StringView vert_path = {
        .data = VERTEX_SHADER_PATH,
        .size = sizeof(VERTEX_SHADER_PATH)
    };
    StringView frag_path = {
        .data = FRAGMENT_SHADER_PATH,
        .size = sizeof(FRAGMENT_SHADER_PATH)
    };
    ShaderMgr shader_mgr;
    ShaderMgrError shader_mgr_err;
    shader_mgr_err = shader_mgr_init(&shader_mgr, vert_path, frag_path, sizeof(tmp_buf), tmp_buf);
    switch (shader_mgr_err) {
        case SHADER_MGR_ERROR_COMPILE_VERT_SHADER:
            SDL_Log("Vert %s\n", tmp_buf);
            return -1;
        case SHADER_MGR_ERROR_COMPILE_FRAG_SHADER:
            SDL_Log("Frag %s\n", tmp_buf);
            return -1;
        case SHADER_MGR_ERROR_LINK_PROGRAM:
            SDL_Log("%s\n", "LINK");
            return -1;
        default:
    }
    GLuint prog;
    shader_mgr_err = shader_mgr_get_program(&shader_mgr, &prog, sizeof(tmp_buf), tmp_buf);
    if (shader_mgr_err != 0) {
        SDL_Log("%s\n", tmp_buf);
        return -1;
    }

    int num_keys;
    kb_state = SDL_GetKeyboardState(&num_keys);
    if (num_keys < ARRAY_LEN(prev_kb_state)) {
        SDL_Log("Unexpectedly large SDL_GetKeyboardState output: %d\n", num_keys);
        retval = -1;
        goto destroy_gl_ctx_lbl;
    }
    gl_init(ARRAY_LEN(meshes), meshes, vaos, vbos_ebos);
    MeshHandle handle;
    u32 cube_verts_cnt = ARRAY_LEN(cube_verts);
    u32 cube_indices_cnt = ARRAY_LEN(cube_indices);
    // u32 cube_indices_cnt = 3;
    Vertex *verts_arr[] = { cube_verts };
    GLuint *indices_arr[] = { cube_indices };
    gl_mesh_init(1, &handle, verts_arr, indices_arr, &cube_verts_cnt, &cube_indices_cnt);
    SDL_Event ev;
    bool quit = false;
    u64 last = SDL_GetTicks();
    u8 watch_frame_counter = 0;
    SDL_SetWindowRelativeMouseMode(win, true);
    while (!quit) {
        f32 dx = 0;
        f32 dy = 0;
        u64 cur_time = SDL_GetTicks();
        UNUSED(last-cur_time);
        watch_frame_counter++;
        if (watch_frame_counter > 60) {
            bool reloaded;
            shader_mgr_err = shader_mgr_reload_if_needed(&shader_mgr, &reloaded);
            if (reloaded) {
                SDL_Log("%s\n", "Reload");
            }
        }
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                    quit = true;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    glViewport(0, 0, ev.window.data1, ev.window.data2);
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    // SDL_Log("%s\n", "move");
                    if (SDL_GetWindowRelativeMouseMode(win)){
                        dx = ev.motion.xrel;
                        dy = ev.motion.yrel;
                        SDL_Log("Move is (%f, %f)\n", dx, dy);
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (ev.button.button == 1) {
                        SDL_SetWindowRelativeMouseMode(win, true);
                    }
                    break;
                default:
            }
        }
        if (is_key_pressed(SDL_SCANCODE_ESCAPE)) {
            SDL_SetWindowRelativeMouseMode(win, false);
        }
        if (is_key_pressed(SDL_SCANCODE_Q)) {
            quit = true;
        }
        (void)is_key_just_pressed;
        memcpy(prev_kb_state, kb_state, num_keys);

        glUseProgram(prog);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        gl_mesh_draw(handle);
        SDL_GL_SwapWindow(win);
    }

destroy_gl_ctx_lbl:
    SDL_GL_DestroyContext(gl_ctx);
destroy_win_lbl:
    SDL_DestroyWindow(win);
quit_sdl_lbl:
    SDL_Quit();
return_lbl:
    return retval;
}

static bool is_key_pressed(SDL_Scancode code) {
    return kb_state[code];
}

static bool is_key_just_pressed(SDL_Scancode code) {
    return kb_state[code] && !prev_kb_state[code];
}

static void game_object_draw(const GameObject *obj, const Camera *cam) {
    const Transform *const tr = &obj->transform;
    const Matrix translation = MatrixTranslate(tr->position.x, tr->position.y, tr->position.z);
    const Matrix scale = MatrixScale(tr->scale.x, tr->scale.y, tr->scale.z);
    const Matrix rotation = QuaternionToMatrix(tr->rotation);
    const Matrix model = MatrixMultiply(MatrixMultiply(scale, translation), rotation); 
    const Matrix view = MatrixLookAt(cam->eye, cam->traget, cam->up);
    // const Matrix total_transform = mul
}
