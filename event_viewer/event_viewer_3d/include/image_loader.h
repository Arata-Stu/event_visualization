#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "types.h" 

// ImageLoaderに必要な設定をまとめた構造体
struct ImageLoaderConfig {
    fs::path timestamps_path;
    fs::path images_dir_path;
    std::string image_extension;
};

class ImageLoader {
public:
    explicit ImageLoader(const ImageLoaderConfig& config);

    std::vector<RGBFrame> load_image_data();

private:
    ImageLoaderConfig config_; 
};