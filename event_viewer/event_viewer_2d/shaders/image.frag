#version 330 core
out vec4 FragColor;

in vec2 v_TexCoord;

uniform sampler2D u_texture; // C++から渡されるテクスチャ
uniform float u_alpha;       // C++から渡される透明度

void main() {
    FragColor = texture(u_texture, v_TexCoord);
    FragColor.a *= u_alpha; // 透明度を適用
}