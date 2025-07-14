#pragma once
#include <string>
#include <vector>
#include <cstdint>

// 1フレームの画像情報を保持する構造体
struct RGBFrame {
    int64_t timestamp;      // タイムスタンプ (us)
    std::string image_path; // 画像ファイルのフルパス
};

class ImageLoader {
public:
    // コンストラクタ：画像データセットのベースパスを受け取る
    ImageLoader(const std::string& base_path);

    // タイムスタンプと画像パスを読み込んで、そのリストを返す
    std::vector<RGBFrame> load_image_data();

private:
    std::string base_path_;
    std::string timestamps_path_;
    std::string images_dir_path_;
};