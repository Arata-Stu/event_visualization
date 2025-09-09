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

struct EventCD;
struct RGBFrame;

namespace fs = std::filesystem;

struct CLIConfig {
    fs::path config_filepath;
    int downsample_factor = 1;
};

CLIConfig parse_arguments(int argc, char* argv[]);
std::vector<EventCD> downsample_events(const std::vector<EventCD>& all_events, int factor);
Resolution calculate_resolution(const std::vector<EventCD>& events);

int main(int argc, char* argv[]) {
    try {
        CLIConfig cli_config = parse_arguments(argc, argv);

        std::cout << "--- Loading master config from: " << cli_config.config_filepath.string() << " ---" << std::endl;
        YAML::Node master_config = YAML::LoadFile(cli_config.config_filepath.string());

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

        std::vector<EventCD> events_to_render = downsample_events(all_events, cli_config.downsample_factor);

        if (events_to_render.empty()) {
             std::cerr << "Error: No events to render after downsampling." << std::endl;
             return -1;
        }

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

        Resolution resolution = calculate_resolution(events_to_render);
        std::cout << "--- Detected resolution: " << resolution.width << "x" << resolution.height << " ---" << std::endl;

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

// (以下、parse_arguments, downsample_events, calculate_resolution 関数の実装は省略)