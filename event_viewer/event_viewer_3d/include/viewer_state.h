#pragma once

// 表示モードを管理するための列挙型
enum class DisplayMode {
    EVENTS_AND_RGB,
    EVENTS_ONLY,
    RGB_ONLY
};

struct ViewerState {
    // 再生制御
    float playback_speed = 1.0f;
    bool is_paused = false;

    // 表示制御
    bool show_bounding_box = true;
    DisplayMode display_mode = DisplayMode::EVENTS_AND_RGB;
    float image_alpha = 0.8f;
    float depth_scale = 1.0f;
    double time_window_us = 2000000.0; // 2秒
};