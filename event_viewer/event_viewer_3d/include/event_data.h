#pragma once
#include <cstdint>

// HDF5から読み込む生データ
struct EventCD {
    uint16_t x, y;
    uint32_t t;
    uint8_t p;
};

// OpenGLで描画するための頂点データ
struct Vertex {
    float x, y, z;          // 座標
    uint8_t r, g, b, a;     // 色
};