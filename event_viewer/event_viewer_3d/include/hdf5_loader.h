#pragma once
#include "types.h"
#include <string>
#include <vector>
#include <H5Cpp.h>

class HDF5Loader {
public:
    HDF5Loader(const std::string& filepath);
    ~HDF5Loader();
    int64_t load_t_offset();
    std::vector<EventCD> load_all_events();

private:
    H5::H5File file;
};