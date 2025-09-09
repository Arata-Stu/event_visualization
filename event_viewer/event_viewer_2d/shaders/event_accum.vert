#version 330 core
layout (location = 0) in vec2 a_pos;
layout (location = 1) in double a_timestamp;
layout (location = 2) in uint a_polarity;

uniform float u_current_time;
uniform float u_time_window;

out float v_polarity;

void main() {
    float age = u_current_time - float(a_timestamp);

    if (age < 0.0 || age > u_time_window) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    } else {
        gl_Position = vec4(a_pos, 0.0, 1.0);
        gl_PointSize = 1.0;
        v_polarity = float(a_polarity);
    }
}