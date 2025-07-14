#version 330 core
layout (location = 0) in vec3 aPos;   // x, y, z(生のタイムスタンプ)
layout (location = 1) in vec4 aColor;

out vec4 v_Color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float u_time;
uniform float u_max_age;

void main() {
    float event_time = aPos.z;
    float age = u_time - event_time;

    // 表示期間内にあるかどうかの判定はそのまま
    if (age > 0.0 && age < u_max_age) {
        // ★★★ Z座標を固定値(0.1)にして、描画順序を制御 ★★★
        gl_Position = projection * view * model * vec4(aPos.x, aPos.y, 0.1, 1.0);
        v_Color = aColor;
    } else {
        // 範囲外のイベントは描画しない
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    }
    
    gl_PointSize = 1.0;
}