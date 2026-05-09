
#pragma once

#include <vector>
//#include <cstdint>
//#include <algorithm>
//#include <numeric>
//#include <unordered_map>

struct Color {
    uint8_t r, g, b;
};

struct HistColor {
    uint8_t r, g, b;
    uint32_t count;
};

std::vector<Color> median_cut_histogram(const std::vector<HistColor>& hist, int target_colors);
