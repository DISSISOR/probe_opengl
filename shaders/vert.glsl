#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aColor;

uniform mat4 model;
uniform mat4 proj_view;

out vec3 ourColor;

void main() {
    ourColor = aColor;
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1);
    gl_Position *= model * proj_view;
    // gl_Position.w = 1;
    // gl_Position += 0.2;
    // gl_Position *= transform;
    // gl_Position.z += 1.9;
    // gl_Position.x -= 0.2;
}
