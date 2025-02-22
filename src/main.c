#include <stdbool.h>

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include "common.h"
#include "arena.h"
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
static Matrix proj_view;
static void game_object_draw(const GameObject *obj);

static void camera_yaw(Camera *cam, float angle);
static void camera_pitch(Camera *cam, float angle);

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

enum { PERSIST_ARENA_SIZE = 1024 * 1024 * 1024 };
u8 persist_arena_buf[ PERSIST_ARENA_SIZE ];
u8 tmp_arena_buf[1024 * 1024 * 5];

Arena g_arena;
Arena tmp_arena;
StringView shader_log;

const StringView vertex_shader_path = SV_FROM_LIT_Z("shaders/vert.glsl");
const StringView fragment_shader_path = SV_FROM_LIT_Z("shaders/frag.glsl");

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    arena_init(&g_arena, persist_arena_buf, sizeof(persist_arena_buf));
    arena_init(&tmp_arena, tmp_arena_buf, sizeof(tmp_arena_buf));
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
    ShaderMgr shader_mgr;
    ShaderMgrError shader_mgr_err;

    Arena restore = g_arena;
    shader_mgr_err = shader_mgr_init(&shader_mgr, vertex_shader_path, fragment_shader_path, &g_arena, &shader_log);
    switch (shader_mgr_err) {
        case SHADER_MGR_ERROR_COMPILE_VERT_SHADER:
            SDL_Log("Vert: " SV_FSPEC "\n", SV_FARGS(shader_log));
            return -1;
        case SHADER_MGR_ERROR_COMPILE_FRAG_SHADER:
            SDL_Log("Frag: " SV_FSPEC "\n", SV_FARGS(shader_log));
            return -1;
        case SHADER_MGR_ERROR_LINK_PROGRAM:
            SDL_Log("%s\n", "LINK");
            return -1;
        default:
    }
    g_arena = restore;

    shader_mgr_err = shader_mgr_get_program(&shader_mgr, &prog, &g_arena, &shader_log);
    if (shader_mgr_err != 0) {
        SDL_Log(SV_FSPEC "\n", SV_FARGS(shader_log));
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
            .position = { 1, 1, 1 },
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
    enum { FRAME_ARENA_SIZE = 1024 * 1024 * 6 };
    u8* frame_arena_buf = ARENA_MAKE(&g_arena, u8, FRAME_ARENA_SIZE);
    Arena frame_arena;
    arena_init(&frame_arena, frame_arena_buf, FRAME_ARENA_SIZE);
    while (!quit) {
        arena_clear(&frame_arena);
        f32 dx = 0;
        f32 dy = 0;
        u64 cur_time = SDL_GetTicks();
        UNUSED(last-cur_time);
        watch_frame_counter++;
        if (watch_frame_counter > 60) {
            bool reloaded;
            shader_mgr_err = shader_mgr_reload_if_needed(&shader_mgr, &reloaded, &frame_arena, &shader_log);
            if (shader_mgr_err != SHADER_MGR_ERROR_NONE) {
                SDL_Log("Shader reload err: " SV_FSPEC "\n", SV_FARGS(shader_log));
            }
            if (reloaded) {
                shader_mgr_err = shader_mgr_get_program(&shader_mgr, &prog, &frame_arena, &shader_log);
                if (shader_mgr_err != 0) {
                    SDL_Log("Shader reload error: " SV_FSPEC  "\n", SV_FARGS(shader_log));
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
                    if (SDL_GetWindowRelativeMouseMode(win)){
                        dx = ev.motion.xrel;
                        dy = ev.motion.yrel;
                        float cam_move_factor = 0.007;
                        camera_pitch(&cam, -dy * cam_move_factor);
                        camera_yaw(&cam, -dx * cam_move_factor);
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
        if (is_key_pressed(SDL_SCANCODE_LSHIFT)) {
            vel.y -= 1;
        }
        if (is_key_pressed(SDL_SCANCODE_SPACE)) {
            vel.y += 1;
        }
        if (is_key_just_pressed(SDL_SCANCODE_R)) {
            memset(&cam.eye, 0, sizeof(cam.eye));
        }
        if (is_key_just_pressed(SDL_SCANCODE_T)) {
            cam.target = cube.transform.position;
        }
        const float speed = 0.03;
        vel = Vector3Scale(Vector3Normalize(vel), speed);

        Vector3 cam_forward = Vector3Subtract(cam.target, cam.eye);
        Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam_forward, cam.up));
        cam_right = Vector3Scale(cam_right, vel.x);

        cam.eye = Vector3Add(cam_right, cam.eye);
        cam.target = Vector3Add(cam_right, cam.target);

        cam_forward = Vector3Scale(cam_forward, vel.z);
        cam.eye = Vector3Add(cam_forward, cam.eye);
        cam.target = Vector3Add(Vector3Scale(cam_forward, vel.z), cam.target);

        cam.eye = Vector3Add(cam.eye, Vector3Scale(cam.up, vel.y));
        cam.target = Vector3Add(cam.target, Vector3Scale(cam.up, vel.y));

        if (vel.x != 0|| vel.y != 0 || vel.z != 0) {
            SDL_Log("Coord: (%f, %f, %f)\n", cam.eye.x, cam.eye.y, cam.eye.z);
            SDL_Log("cam_right: (%f, %f, %f)\n", cam_right.x, cam_right.y, cam_right.z);
        }
        (void)is_key_just_pressed;
        memcpy(prev_kb_state, kb_state, num_keys);

        const Matrix proj = MatrixPerspective(DEG2RAD * 45, 800.f/600.f, 0.1, 100);
        const Matrix view = MatrixLookAt(cam.eye, cam.target, cam.up);
        proj_view = MatrixMultiply(view, proj);
        glUniformMatrix4fv(glGetUniformLocation(prog, "proj_view"), 1, GL_FALSE, &proj_view.m0);
        glUseProgram(prog);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        game_object_draw(&cube);
        game_object_draw(&floor);
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

static void camera_yaw(Camera *cam, float angle) {
    const Vector3 cam_forward = Vector3Normalize(Vector3Subtract(cam->eye, cam->target));
    const Vector3 new_forward = Vector3RotateByAxisAngle(cam_forward, cam->up, angle);
    cam->target = Vector3Add(cam->eye, new_forward);
}

static void camera_pitch(Camera *cam, float angle) {
    const Vector3 cam_forward = Vector3Subtract(cam->eye, cam->target);
    const Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam->up, cam_forward));
    const Vector3 new_forward = Vector3RotateByAxisAngle(cam_forward, cam_right, angle);
    cam->target = Vector3Add(cam->eye, new_forward);
}

static void game_object_draw(const GameObject *obj) {
    UNUSED(cam);
    const Transform *const tr = &obj->transform;
    const Matrix translation = MatrixTranslate(tr->position.x, tr->position.y, tr->position.z);
    const Matrix scale = MatrixScale(tr->scale.x, tr->scale.y, tr->scale.z);
    const Matrix model = MatrixMultiply(translation, scale);
    glUniformMatrix4fv(glGetUniformLocation(prog, "model"), 1, GL_FALSE, &model.m0);
    gl_mesh_draw(obj->mesh);
}
