#include "image_loader.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem> // C++17のfilesystemライブラリを使用

namespace fs = std::filesystem;

ImageLoader::ImageLoader(const std::string& base_path) : base_path_(base_path) {
    timestamps_path_ = fs::path(base_path_) / "timestamps.txt";
    images_dir_path_ = fs::path(base_path_) / "left" / "distorted";
}

std::vector<RGBFrame> ImageLoader::load_image_data() {
    std::vector<RGBFrame> frames;

    // 1. timestamps.txt からタイムスタンプを読み込む
    std::vector<int64_t> timestamps;
    std::ifstream ts_file(timestamps_path_);
    if (!ts_file.is_open()) {
        std::cerr << "Error: Cannot open timestamps file: " << timestamps_path_ << std::endl;
        return {};
    }
    std::string line;
    while (std::getline(ts_file, line)) {
        try {
            timestamps.push_back(std::stoll(line));
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not parse timestamp: " << line << std::endl;
        }
    }
    ts_file.close();
    std::cout << "--- " << timestamps.size() << "個のタイムスタンプを読み込みました ---" << std::endl;


    // 2. 画像ディレクトリから画像ファイルパスを取得する
    std::vector<std::string> image_paths;
    if (!fs::exists(images_dir_path_) || !fs::is_directory(images_dir_path_)) {
        std::cerr << "Error: Image directory not found: " << images_dir_path_ << std::endl;
        return {};
    }
    for (const auto& entry : fs::directory_iterator(images_dir_path_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            image_paths.push_back(entry.path().string());
        }
    }
    // ファイル名でソートして、タイムスタンプとの順序を保証する
    std::sort(image_paths.begin(), image_paths.end());
    std::cout << "--- " << image_paths.size() << "個の画像ファイルを検出しました ---" << std::endl;


    // 3. タイムスタンプと画像パスを紐付ける
    if (timestamps.size() != image_paths.size()) {
        std::cerr << "Warning: Timestamp count (" << timestamps.size() 
                  << ") does not match image file count (" << image_paths.size() << ")!" << std::endl;
    }

    size_t num_frames = std::min(timestamps.size(), image_paths.size());
    frames.reserve(num_frames);
    for (size_t i = 0; i < num_frames; ++i) {
        frames.push_back({timestamps[i], image_paths[i]});
    }

    std::cout << "--- " << frames.size() << "個の画像フレーム情報を正常に読み込みました ---" << std::endl;
    return frames;
}