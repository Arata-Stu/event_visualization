#include "hdf5_loader.h"
#include "renderer.h" 
#include "image_loader.h"
#include "yaml-cpp/yaml.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <filesystem>

// 必要な前方宣言や構造体定義
struct EventCD;
struct RGBFrame;

namespace fs = std::filesystem;

// --- 構造体の定義 ---
struct CLIConfig {
    fs::path config_filepath;
    int downsample_factor = 1;
};

// --- 関数のプロトタイプ宣言 ---
CLIConfig parse_arguments(int argc, char* argv[]);
std::vector<EventCD> downsample_events(const std::vector<EventCD>& all_events, int factor);
Resolution calculate_resolution(const std::vector<EventCD>& events);


// --- main関数 ---
int main(int argc, char* argv[]) {
    try {
        // 1. コマンドライン引数を解析
        CLIConfig cli_config = parse_arguments(argc, argv);

        // 2. マスター設定ファイル(YAML)を読み込む
        std::cout << "--- Loading master config from: " << cli_config.config_filepath.string() << " ---" << std::endl;
        YAML::Node master_config = YAML::LoadFile(cli_config.config_filepath.string());

        // 3. HDF5ファイルからイベントを読み込む
        if (!master_config["event_file"]) {
            throw std::runtime_error("'event_file' not found in master config.");
        }
        fs::path h5_filepath = cli_config.config_filepath.parent_path() / master_config["event_file"].as<std::string>();
        
        HDF5Loader h5_loader(h5_filepath.string());
        int64_t t_offset = h5_loader.load_t_offset();
        std::vector<EventCD> all_events = h5_loader.load_all_events();

        if (all_events.empty()) {
            std::cerr << "Error: No events found in the HDF5 file." << std::endl;
            return -1;
        }

        // 4. イベントデータをダウンサンプリング
        std::vector<EventCD> events_to_render = downsample_events(all_events, cli_config.downsample_factor);

        if (events_to_render.empty()) {
             std::cerr << "Error: No events to render after downsampling." << std::endl;
             return -1;
        }

        // 5. RGB画像データを読み込み
        std::vector<RGBFrame> all_images;
        if (master_config["rgb_images"]) {
            YAML::Node rgb_config_node = master_config["rgb_images"];
            ImageLoaderConfig image_loader_config;
            fs::path rgb_base_path = cli_config.config_filepath.parent_path() / rgb_config_node["base_path"].as<std::string>();
            
            image_loader_config.timestamps_path = rgb_base_path / rgb_config_node["timestamps_file"].as<std::string>();
            image_loader_config.images_dir_path = rgb_base_path / rgb_config_node["image_directory"].as<std::string>();
            image_loader_config.image_extension = rgb_config_node["image_extension"].as<std::string>();

            ImageLoader image_loader(image_loader_config);
            all_images = image_loader.load_image_data();
        }

        // 6. データからセンサーの解像度を計算
        Resolution resolution = calculate_resolution(events_to_render);
        std::cout << "--- Detected resolution: " << resolution.width << "x" << resolution.height << " ---" << std::endl;

        // 7. レンダラーを実行
        run_renderer(events_to_render, all_images, resolution.width, resolution.height, t_offset);

    } catch (const H5::Exception& err) {
        std::cerr << "A fatal HDF5 error occurred." << std::endl;
        err.printErrorStack();
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nProgram finished successfully." << std::endl;
    return 0;
}


// --- ★★★ここからが省略されていた関数の実装です★★★ ---

CLIConfig parse_arguments(int argc, char* argv[]) {
    if (argc < 2) {
        throw std::runtime_error("Usage: " + std::string(argv[0]) + " <path_to_run_config.yaml> [downsample_factor]");
    }
    
    CLIConfig config;
    config.config_filepath = argv[1];

    if (argc >= 3) {
        try {
            config.downsample_factor = std::stoi(argv[2]);
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("Error: Invalid downsample_factor '" + std::string(argv[2]) + "'. Must be an integer.");
        }
    }
    if (config.downsample_factor <= 0) {
        config.downsample_factor = 1;
    }
    return config;
}

std::vector<EventCD> downsample_events(const std::vector<EventCD>& all_events, int factor) {
    if (factor <= 1) {
        return all_events;
    }
    std::cout << "--- Original event count: " << all_events.size() << std::endl;
    std::cout << "--- Downsampling by a factor of " << factor << "..." << std::endl;
    std::vector<EventCD> downsampled;
    downsampled.reserve(all_events.size() / factor + 1);
    for (size_t i = 0; i < all_events.size(); i += factor) {
        downsampled.push_back(all_events[i]);
    }
    std::cout << "--- Downsampled event count: " << downsampled.size() << std::endl;
    return downsampled;
}

Resolution calculate_resolution(const std::vector<EventCD>& events) {
    if (events.empty()) {
        return {0, 0};
    }
    uint16_t max_x = 0;
    uint16_t max_y = 0;
    std::cout << "--- Calculating sensor resolution from data..." << std::endl;
    for (const auto& event : events) {
        max_x = std::max(max_x, event.x);
        max_y = std::max(max_y, event.y);
    }
    return {static_cast<int>(max_x + 1), static_cast<int>(max_y + 1)};
}