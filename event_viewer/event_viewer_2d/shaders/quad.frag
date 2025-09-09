#version 330 core

// 頂点シェーダーから受け取るテクスチャ座標
in vec2 v_tex_coord;

// 最終的に出力するピクセルの色
out vec4 FragColor;

// C++側から受け取る uniform 変数
uniform sampler2D u_texture; // 描画するテクスチャ
uniform float u_alpha;       // 全体に適用する透明度

void main() {
    // 指定された座標のテクスチャの色を取得
    vec4 tex_color = texture(u_texture, v_tex_coord);
    
    // テクスチャの色に、指定された透明度を乗算して出力
    FragColor = vec4(tex_color.rgb, tex_color.a * u_alpha);
}