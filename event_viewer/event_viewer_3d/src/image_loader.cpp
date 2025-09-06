#include "image_loader.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;

// コンストラクタは、main関数で組み立てられた設定構造体をメンバ変数にコピーします
ImageLoader::ImageLoader(const ImageLoaderConfig& config) : config_(config) {}

std::vector<RGBFrame> ImageLoader::load_image_data() {
    std::vector<RGBFrame> frames;

    // 1. timestamps.txt からタイムスタンプを読み込む
    std::vector<int64_t> timestamps;
    std::ifstream ts_file(config_.timestamps_path);
    if (!ts_file.is_open()) {
        std::cerr << "Error: Cannot open timestamps file: " << config_.timestamps_path << std::endl;
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
    std::cout << "--- Loaded " << timestamps.size() << " timestamps ---" << std::endl;


    // 2. 画像ディレクトリから画像ファイルパスを取得する
    std::vector<std::string> image_paths;
    if (!fs::exists(config_.images_dir_path) || !fs::is_directory(config_.images_dir_path)) {
        std::cerr << "Error: Image directory not found: " << config_.images_dir_path << std::endl;
        return {};
    }
    for (const auto& entry : fs::directory_iterator(config_.images_dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == config_.image_extension) {
            image_paths.push_back(entry.path().string());
        }
    }
    // ファイル名でソートして、タイムスタンプとの順序を保証する
    std::sort(image_paths.begin(), image_paths.end());
    std::cout << "--- Found " << image_paths.size() << " image files with extension '" << config_.image_extension << "' ---" << std::endl;


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

    std::cout << "--- Successfully loaded " << frames.size() << " image frames ---" << std::endl;
    return frames;
}   