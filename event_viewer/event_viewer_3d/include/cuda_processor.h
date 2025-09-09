#pragma once
#include "types.h"
#include <vector>
#include <GL/glew.h>

void init_cuda_for_gl();
void register_gl_buffer(GLuint vbo);
void unregister_gl_buffer();

unsigned int process_all_events(const std::vector<EventCD>& all_events, int width, int height, int64_t t_offset, double base_time, const ColorConfig& colors);