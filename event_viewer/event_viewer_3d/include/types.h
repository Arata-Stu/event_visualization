// types.h
#pragma once

#include <string>
#include <cstdint>
#include <filesystem>

// HDF5からのイベントデータ
struct EventCD {
    uint64_t p;
    uint16_t x;
    uint16_t y;
    uint8_t pol;
    uint64_t t;
};

// レンダリング用の頂点データ
struct Vertex {
    float x, y, z;
    uint8_t r, g, b, a;
    float timestamp;
};

// RGB画像フレームの情報
struct RGBFrame {
    int64_t timestamp;
    std::string image_path;
};

// ImageLoaderの設定
namespace fs = std::filesystem;
struct ImageLoaderConfig {
    fs::path timestamps_path;
    fs::path images_dir_path;
    std::string image_extension;
};

// センサーの解像度
struct Resolution {
    int width = 0;
    int height = 0;
};