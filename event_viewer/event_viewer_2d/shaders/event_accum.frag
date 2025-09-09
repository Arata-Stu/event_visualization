#version 330 core
in float v_polarity;
out vec4 FragColor;

void main() {
    // 極性に応じてカウンター情報をエンコード
    if (v_polarity > 0.5) {
        // Polarity ON: Rチャンネルに+1する
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else {
        // Polarity OFF: Gチャンネルに+1する
        FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    }
}