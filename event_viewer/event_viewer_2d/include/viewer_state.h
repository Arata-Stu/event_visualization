#pragma once

enum class DisplayMode {
    EVENTS_AND_RGB = 0,
    EVENTS_ONLY = 1,
    RGB_ONLY = 2,
};

struct ViewerState {
    float playback_speed = 1.0f;
    bool is_paused = false;

    DisplayMode display_mode = DisplayMode::EVENTS_AND_RGB;
    float rgb_alpha = 0.7f;
    float event_alpha = 1.0f;
    double time_window_us = 20000.0; // 20ms
};