#version 330 core
layout (location = 0) in vec3 aPos;   // x, y, z(生のタイムスタンプ)
layout (location = 1) in vec4 aColor;

out vec4 v_Color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float u_time;
uniform float u_max_age; // ★★★ 時間幅をuniformで受け取る ★★★

void main() {
    float event_time = aPos.z;
    float age = u_time - event_time;
    // float max_age = 2000000.0; // 固定値を削除

    if (age > 0.0 && age < u_max_age) {
        float normalized_age = age / u_max_age; // ★★★ 動的な時間幅で正規化 ★★★
        float display_z = 1.0 - 2.0 * normalized_age;
        
        gl_Position = projection * view * model * vec4(aPos.x, aPos.y, display_z, 1.0);
        v_Color = aColor;
    } else {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    }
    
    gl_PointSize = 1.0;
}