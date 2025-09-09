#version 330 core
layout (location = 0) in vec2 a_pos;       // 頂点座標 (x, y)
layout (location = 1) in vec2 a_tex_coord; // テクスチャ座標 (u, v)

// C++側から受け取る行列
uniform mat4 projection;
uniform mat4 view;

// フラグメントシェーダーに渡すテクスチャ座標
out vec2 v_tex_coord;

void main() {
    // 2Dなのでモデル行列は不要 (単位行列とみなす)
    // 最終的な頂点位置 = projection * view * position
    gl_Position = projection * view * vec4(a_pos, 0.0, 1.0);
    
    // テクスチャ座標をそのままフラグメントシェーダーへ
    v_tex_coord = a_tex_coord;
}