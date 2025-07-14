#pragma once
#include "event_data.h"
#include "image_loader.h" 
#include <vector>

void run_renderer(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int width, int height, int64_t t_offset);