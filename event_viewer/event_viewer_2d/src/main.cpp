#include "hdf5_loader.h"
#include "renderer.h" 
#include "image_loader.h"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <filesystem>

// Forward declarations
struct EventCD;
struct RGBFrame;

namespace fs = std::filesystem;

// Command line configuration
struct CLIConfig {
    fs::path config_filepath;
    int downsample_factor = 1;
};

// Function prototypes
CLIConfig parse_arguments(int argc, char* argv[]);
std::vector<EventCD> downsample_events(const std::vector<EventCD>& all_events, int factor);
Resolution calculate_resolution(const std::vector<EventCD>& events);


// --- Main Function ---
int main(int argc, char* argv[]) {
    try {
        // 1. Parse command line arguments
        CLIConfig cli_config = parse_arguments(argc, argv);

        // 2. Load master YAML config file
        std::cout << "--- Loading master config from: " << cli_config.config_filepath.string() << " ---" << std::endl;
        YAML::Node master_config = YAML::LoadFile(cli_config.config_filepath.string());

        // 3. Load events from HDF5 file
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

        // 4. Downsample event data if requested
        std::vector<EventCD> events_to_render = downsample_events(all_events, cli_config.downsample_factor);

        if (events_to_render.empty()) {
             std::cerr << "Error: No events to render after downsampling." << std::endl;
             return -1;
        }

        // 5. Load RGB image data if specified
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

        // 6. Load color configuration from YAML, with defaults
        glm::vec3 bg_color(1.0f, 1.0f, 1.0f);   // Default: White
        glm::vec3 on_color(1.0f, 0.0f, 0.0f);   // Default: Red
        glm::vec3 off_color(0.0f, 0.0f, 1.0f);  // Default: Blue

        if (master_config["colors"]) {
            YAML::Node colors = master_config["colors"];
            if (colors["background"]) bg_color = glm::vec3(colors["background"][0].as<float>(), colors["background"][1].as<float>(), colors["background"][2].as<float>());
            if (colors["event_on"])   on_color = glm::vec3(colors["event_on"][0].as<float>(), colors["event_on"][1].as<float>(), colors["event_on"][2].as<float>());
            if (colors["event_off"])  off_color = glm::vec3(colors["event_off"][0].as<float>(), colors["event_off"][1].as<float>(), colors["event_off"][2].as<float>());
        }

        // 7. Calculate sensor resolution from data
        Resolution resolution = calculate_resolution(events_to_render);
        std::cout << "--- Detected resolution: " << resolution.width << "x" << resolution.height << " ---" << std::endl;

        // 8. Run the renderer with all loaded data and configuration
        run_renderer(events_to_render, all_images, resolution.width, resolution.height, t_offset, bg_color, on_color, off_color);

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


// --- Function Implementations ---

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