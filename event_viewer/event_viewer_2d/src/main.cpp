#include "hdf5_loader.h"
#include "renderer.h"
#include "image_loader.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

int main(int argc, char* argv[]) {
    // --- 引数のチェック ---
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_hdf5_file> [path_to_rgb_data] [downsample_factor]" << std::endl;
        std::cerr << "Example (Events only): " << argv[0] << " events.h5 10" << std::endl;
        std::cerr << "Example (Events + RGB): " << argv[0] << " events.h5 /path/to/images 10" << std::endl;
        return 1;
    }

    std::string h5_filepath = argv[1];
    std::string rgb_path = "";
    int downsample_factor = 1;

    // --- 引数の数を元に、RGBパスとダウンサンプリング係数を判断 ---
    if (argc == 3) {
        try {
            // 3番目の引数が数字ならダウンサンプリング係数とみなす
            downsample_factor = std::stoi(argv[2]);
        } catch (const std::exception& e) {
            // 数字でなければRGBパスとみなす
            rgb_path = argv[2];
            downsample_factor = 1;
        }
    } else if (argc >= 4) {
        rgb_path = argv[2];
        try {
            downsample_factor = std::stoi(argv[3]);
        } catch (const std::exception& e) {
            std::cerr << "Error: Invalid downsample_factor '" << argv[3] << "'. Must be an integer." << std::endl;
            return 1;
        }
    }
    
    if (downsample_factor <= 0) {
        downsample_factor = 1;
    }

    // --- HDF5ファイルの読み込み ---
    try {
        HDF5Loader loader(h5_filepath);
        int64_t t_offset = loader.load_t_offset();
        std::vector<EventCD> all_events = loader.load_all_events();

        if (all_events.empty()) {
            std::cerr << "No events found in the HDF5 file." << std::endl;
            return -1;
        }

        std::vector<EventCD> events_to_render;

        // --- ダウンサンプリング処理 ---
        if (downsample_factor > 1) {
            std::cout << "--- Original event count: " << all_events.size() << std::endl;
            std::cout << "--- Downsampling by a factor of " << downsample_factor << "..." << std::endl;
            
            events_to_render.reserve(all_events.size() / downsample_factor + 1);
            for (size_t i = 0; i < all_events.size(); i += downsample_factor) {
                events_to_render.push_back(all_events[i]);
            }
            std::cout << "--- Downsampled event count: " << events_to_render.size() << std::endl;
        } else {
            events_to_render = std::move(all_events);
        }

        if (events_to_render.empty()) {
             std::cerr << "No events to render after downsampling." << std::endl;
             return -1;
        }
        
        // --- RGB画像情報の読み込み ---
        std::vector<RGBFrame> all_images; // 空のベクトルで初期化
        if (!rgb_path.empty()) {
            ImageLoader image_loader(rgb_path);
            all_images = image_loader.load_image_data();
            if (all_images.empty()) {
                std::cerr << "Warning: RGB path provided, but no image frames were loaded." << std::endl;
            }
        }

        uint16_t max_x = 0;
        uint16_t max_y = 0;
        std::cout << "--- Calculating sensor resolution from data..." << std::endl;

        size_t event_index = 0; // デバッグ用にイベントのインデックスを追跡
        for (const auto& event : events_to_render) {
            // x座標が想定を超える値だったら、そのイベントの情報をコンソールに出力
            if (event.x >= 640) { // 想定される幅の上限値でチェック
                std::cerr << "!!! Found an unusual event at index: " << event_index << " !!!" << std::endl;
                std::cerr << "    event.t: " << event.t << std::endl;
                std::cerr << "    event.x: " << event.x << "  <-- This value is strange." << std::endl;
                std::cerr << "    event.y: " << event.y << std::endl;
                std::cerr << "    event.p: " << event.p << std::endl;
            }

            max_x = std::max(max_x, event.x);
            max_y = std::max(max_y, event.y);
            event_index++;
        }

        int width = max_x + 1;
        int height = max_y + 1;
        std::cout << "--- Detected resolution: " << width << "x" << height << " ---" << std::endl;

        // --- レンダラーの呼び出し ---
        run_renderer(events_to_render, all_images, width, height, t_offset);

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