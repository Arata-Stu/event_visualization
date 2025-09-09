#pragma once
#include <vector>   
#include "types.h" 

class ImageLoader {
public:
    explicit ImageLoader(const ImageLoaderConfig& config);
    std::vector<RGBFrame> load_image_data();

private:
    ImageLoaderConfig config_;
};