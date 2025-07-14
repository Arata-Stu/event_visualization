#version 330 core
layout (location = 0) in vec3 aPos;       // 板ポリゴンの頂点座標
layout (location = 1) in vec2 aTexCoord;  // テクスチャ座標

out vec2 v_TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    v_TexCoord = aTexCoord;
}