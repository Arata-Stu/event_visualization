#version 330 core
in vec2 v_tex_coord;
out vec4 FragColor;

uniform sampler2D u_texture;

// ★変更: const定義を削除し、uniform変数を追加
uniform vec3 u_on_color;
uniform vec3 u_off_color;

void main() {
    vec4 counts = texture(u_texture, v_tex_coord);

    if (counts.r == 0.0 && counts.g == 0.0) {
        discard;
        return;
    }

    if (counts.r > counts.g) {
        // ONイベントが優位なら u_on_color を使う
        FragColor = vec4(u_on_color, 1.0);
    } else if (counts.g > counts.r) {
        // OFFイベントが優位なら u_off_color を使う
        FragColor = vec4(u_off_color, 1.0);
    } else {
        discard;
    }
}