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
    Vector3 target;
} Camera;

static Camera cam;
static void game_object_draw(const GameObject *obj);

static Vertex cube_verts[] = {
    // Дальние (z+)
    { { 0.5, -0.5,  0.5}, {1, 0}, {0, 0, 1}, {1, 0, 0} },
    { {-0.5, -0.5,  0.5}, {1, 0}, {0, 0, -1}, {1, 0, 0} },
    { {-0.5,  0.5,  0.5}, {1, 1}, {0, 0, -1}, {1, 0, 0} },
    { { 0.5,  0.5,  0.5}, {1, 1}, {0, 0, 1}, {1, 0, 0} },

    // Ближние (z-)
    { { 0.5, -0.5, -0.5}, {0, 0}, {0, 0, 1}, {0, 1, 0} },
    { {-0.5, -0.5, -0.5}, {0, 0}, {0, 0, -1}, {0, 1, 0} },
    { {-0.5,  0.5, -0.5}, {0, 1}, {0, 0, -1}, {0, 1, 0} },
    { { 0.5,  0.5, -0.5}, {0, 1}, {0, 0, 1}, {0, 1, 0} },
};
static_assert(ARRAY_LEN(cube_verts) == 8);
static GLuint cube_indices[] = {
    0, 1, 2, // дальняя грань
    0, 2, 3,

    4, 5, 6, // ближняя грань
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

#define FLOOR_HEIGHT -0.0

static Vertex floor_verts[] = {
    { {-1, FLOOR_HEIGHT, 1}, {0, 0}, {0, 0, 1}, {0.5, 0.5, 0.5}},
    { {1, FLOOR_HEIGHT, 1}, {0, 0}, {0, 0, 1}, {0.5, 0.5, 0.5}},
    { {-1, FLOOR_HEIGHT + 0.5, -1}, {0, 0}, {0, 0, 1}, {0.5, 0.5, 0.5}},
    { {1, FLOOR_HEIGHT + 0.5, -1}, {0, 0}, {0, 0, 1}, {0.5, 0.5, 0.5}},
};

static GLuint floor_indices[] = {
    0, 1, 2,
    1, 2, 3
};

static Mesh meshes[4096];
static GLuint vaos[ARRAY_LEN(meshes)];
static GLuint vbos_ebos [ARRAY_LEN(meshes) * 2];
GLuint prog;

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
    cam.up = (Vector3) {0, 1, 0};
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
    MeshHandle handles[2];
    u32 vert_cnts[] = { ARRAY_LEN(cube_verts), ARRAY_LEN(floor_verts) };
    u32 indices_cnts[] = { ARRAY_LEN(cube_indices), ARRAY_LEN(floor_indices) };
    Vertex *verts_arr[] = { cube_verts, floor_verts };
    GLuint *indices_arr[] = { cube_indices, floor_indices };
    gl_mesh_init(2, handles, verts_arr, indices_arr, vert_cnts, indices_cnts);
    GameObject cube = {
        .mesh = handles[0],
        .transform = {
            .position = { 0.5, 0, 0 },
            .scale = {0.2, 0.2, 0.2},
            .rotation = { 0 }
        }
    };
    GameObject floor = {
        .mesh = handles[1],
        .transform = {
            .position = { 0 },
            .scale = {1, 1, 1},
            .rotation = { 0 },
        }
    };
    SDL_Event ev;
    bool quit = false;
    u64 last = SDL_GetTicks();
    u8 watch_frame_counter = 0;
    SDL_SetWindowRelativeMouseMode(win, true);
    cam.target = (Vector3) { 0, 0, 0 };
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
                shader_mgr_err = shader_mgr_get_program(&shader_mgr, &prog, sizeof(tmp_buf), tmp_buf);
                if (shader_mgr_err != 0) {
                    SDL_Log("Shader reload error: %s\n", tmp_buf);
                }
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
                        float cam_move_factor = 0.001;
                        cam.target.x+=dx * cam_move_factor;
                        cam.target.y+=dy * cam_move_factor;
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
        Vector3 vel = { 0 };
        if (is_key_pressed(SDL_SCANCODE_W)) {
            vel.z += 1;
        }
        if (is_key_pressed(SDL_SCANCODE_S)) {
            vel.z -= 1;
        }
        if (is_key_pressed(SDL_SCANCODE_D)) {
            vel.x += 1;
        }
        if (is_key_pressed(SDL_SCANCODE_A)) {
            vel.x -= 1;
        }
        if (is_key_just_pressed(SDL_SCANCODE_R)) {
            memset(&cam.eye, 0, sizeof(cam.eye));
        }
        vel = Vector3Normalize(vel);
        vel.x *= 0.1;
        vel.y *= 0.1;
        vel.z *= 0.1;
        Vector3 cam_forward = Vector3Subtract(cam.target, cam.eye);
        Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam.up, cam_forward));
        cam_right = Vector3Scale(cam_right, vel.x);
        cam.eye = Vector3Add(cam_right, cam.eye);
        cam.target = Vector3Add(Vector3Scale(cam_right, vel.x), cam.target);

        cam_forward = Vector3Scale(cam_forward, vel.z);
        cam.eye = Vector3Add(cam_forward, cam.eye);
        cam.target = Vector3Add(Vector3Scale(cam_forward, vel.x), cam.target);
        if (vel.x != 0|| vel.y != 0 || vel.z != 0) {
            SDL_Log("Coord: (%f, %f, %f)\n", cam.eye.x, cam.eye.y, cam.eye.z);
            SDL_Log("cam_right: (%f, %f, %f)\n", cam_right.x, cam_right.y, cam_right.z);
        }
        (void)is_key_just_pressed;
        memcpy(prev_kb_state, kb_state, num_keys);

        glUseProgram(prog);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if 1
        game_object_draw(&cube);
        // game_object_draw(&floor);
        UNUSED(floor);
#else
        UNUSED(floor);
        UNUSED(cube);
        UNUSED(game_object_draw);
        {
            const Matrix total_transform = MatrixIdentity();
            glUniformMatrix4fv(glGetUniformLocation(prog, "transform"), 1, GL_FALSE, &total_transform.m0);
            gl_mesh_draw(handles[0]);
            gl_mesh_draw(handles[1]);
        }
#endif
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

static void game_object_draw(const GameObject *obj) {
    UNUSED(cam);
    const Transform *const tr = &obj->transform;
    const Matrix translation = MatrixTranslate(tr->position.x, tr->position.y, tr->position.z);
    const Matrix scale = MatrixScale(tr->scale.x, tr->scale.y, tr->scale.z);
    const Matrix model = MatrixMultiply(translation, scale);
    // const Matrix projection = MatrixFrustum(-0.5, 0.5, -0.5, 0.5, 0.1, 100);
    Matrix view = MatrixLookAt(cam.eye, cam.target, cam.up);
    view = MatrixIdentity();
    Matrix view_model = MatrixMultiply(view, model);
    Matrix total_transform = view_model;
    glUniformMatrix4fv(glGetUniformLocation(prog, "transform"), 1, GL_FALSE, &total_transform.m0);
    gl_mesh_draw(obj->mesh);
}
