#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aColor;

out vec3 ourColor;

void main() {
    ourColor = aColor;
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1);
    // gl_Position.z += 1.9;
    // gl_Position.x -= 0.2;
}
