#version 330 core
in float v_polarity;
out vec4 FragColor;

void main() {
    if (v_polarity > 0.5) {
        FragColor = vec4(1.0, 0.2, 0.2, 1.0); // Polarity ON: Red
    } else {
        FragColor = vec4(0.2, 0.5, 1.0, 1.0); // Polarity OFF: Blue
    }
}