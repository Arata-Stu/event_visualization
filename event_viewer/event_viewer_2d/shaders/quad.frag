#version 330 core
in vec2 v_tex_coord;
out vec4 FragColor;

uniform sampler2D u_texture; // カウンターテクスチャ
uniform float u_alpha;       // (このシェーダーでは使わないが、念のため残す)

// ★追加: 最終的に表示する色を定義
const vec4 COLOR_ON = vec4(1.0, 0.0, 0.0, 1.0); // 赤
const vec4 COLOR_OFF = vec4(0.0, 0.0, 1.0, 1.0); // 青

void main() {
    // カウンターテクスチャからON/OFFのカウント数を取得
    // texture().r が ONのカウント数, texture().g が OFFのカウント数に相当
    vec4 counts = texture(u_texture, v_tex_coord);

    // イベントが全くないピクセルは透明にして背景の白を見せる
    if (counts.r == 0.0 && counts.g == 0.0) {
        discard; // or FragColor = vec4(0.0);
        return;
    }

    // 多数決ロジック
    if (counts.r > counts.g) {
        // ONイベントが優位なら赤
        FragColor = COLOR_ON;
    } else if (counts.g > counts.r) {
        // OFFイベントが優位なら青
        FragColor = COLOR_OFF;
    } else {
        // 同数、またはイベントがない場合は描画しない
        discard; // ピクセルを描画せず、背景色をそのまま表示
    }
}