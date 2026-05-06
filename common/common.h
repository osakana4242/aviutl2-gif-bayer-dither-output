#pragma once

#include <string>
#include <iterator>
#include "median_cut.h"

//---------------------------------------------------------------------
// ColoMode
//---------------------------------------------------------------------

enum class ColorMode {
	C256 = 0,
	C216 = 1,
	C16 = 2,
	C8 = 3,
	Custom = 4,
	Count,
};

static const wchar_t* g_mode_display_names[] = {
	L"パレット256色",
	L"パレット216色",
	L"パレット16色",
	L"パレット8色",
	L"カスタム",
};

static_assert((int)ColorMode::Count == _countof(g_mode_display_names),
	"mode name mismatch");

//---------------------------------------------------------------------
// BayerMode
//---------------------------------------------------------------------

enum class BayerMode {
	Bayer8x8 = 0,
	Bayer4x4 = 1,
	Count,
};

static const wchar_t* g_bayer_display_names[] = {
	L"Bayer 8x8",
	L"Bayer 4x4",
};

static_assert((int)BayerMode::Count == _countof(g_bayer_display_names),
	"bayer name mismatch");

//---------------------------------------------------------------------

struct BayerDitherConfig {
	ColorMode mode = ColorMode::C8;
	BayerMode bayer = BayerMode::Bayer4x4;
	float strength = 2.0;
	int hoge = 1;
	int color_count = 16;
	int color_shift = 0;
};

//---------------------------------------------------------------------
// パレット

// パレット8
inline constexpr unsigned char palette_8[8][3] = {
	{0, 0, 0},
	{0, 0, 255},
	{0, 255, 0},
	{0, 255, 255},
	{255, 0, 0},
	{255, 0, 255},
	{255, 255, 0},
	{255, 255, 255}
};

// パレット16
inline constexpr unsigned char palette_16[16][3] = {
	{0, 0, 0},
	{128, 0, 0},
	{0, 128, 0},
	{128, 128, 0},
	{0, 0, 128},
	{128, 0, 128},
	{0, 128, 128},
	{128, 128, 128},
	{192, 192, 192},
	{255, 0, 0},
	{0, 255, 0},
	{255, 255, 0},
	{0, 0, 255},
	{255, 0, 255},
	{0, 255, 255},
	{255, 255, 255}
};

// パレット216
inline unsigned char palette_216[256][3] = {};

inline void init_palette_216() {
	int idx = 0;

	for (int r = 0; r < 6; r++) {
		for (int g = 0; g < 6; g++) {
			for (int b = 0; b < 6; b++) {
				palette_216[idx][0] = r * 51;
				palette_216[idx][1] = g * 51;
				palette_216[idx][2] = b * 51;
				idx++;
			}
		}
	}
}

// パレット256
inline unsigned char palette_256[256][3] = {};

inline void init_palette_256() {
	int idx = 0;

	// ① WebSafe (216色)
	for (int r = 0; r < 6; r++) {
		for (int g = 0; g < 6; g++) {
			for (int b = 0; b < 6; b++) {
				palette_256[idx][0] = r * 51;
				palette_256[idx][1] = g * 51;
				palette_256[idx][2] = b * 51;
				idx++;
			}
		}
	}

	// ② グレースケール（16色くらい）
	for (int i = 0; i < 16; i++) {
		int v = i * 255 / 15;
		palette_256[idx][0] = v;
		palette_256[idx][1] = v;
		palette_256[idx][2] = v;
		idx++;
	}

	// ③ 補間色（残り）
	while (idx < 256) {
		int t = idx - 232; // 適当な分布
		palette_256[idx][0] = (t * 37) % 256;
		palette_256[idx][1] = (t * 73) % 256;
		palette_256[idx][2] = (t * 109) % 256;
		idx++;
	}
}

// パレットカスタム
inline unsigned char palette_custom[256][3] = {};
inline size_t palette_custom_size = 16;

struct PaletteInitializer {
	PaletteInitializer() {
		init_palette_216();
		init_palette_256();
	}
};
inline PaletteInitializer g_palette_initializer;

inline size_t get_palette(const BayerDitherConfig& config, const unsigned char (*&palette)[3]) {
	switch (config.mode) {
	case ColorMode::C256:
		palette = palette_256;
		return std::size(palette_256);
	case ColorMode::C216:
		palette = palette_216;
		return std::size(palette_216);
	case ColorMode::C16:
		palette = palette_16;
		return std::size(palette_16);
	case ColorMode::C8:
		palette =  palette_8;
		return std::size(palette_8);
	case ColorMode::Custom:
		palette = palette_custom;
		return palette_custom_size;
	default:
		palette = nullptr;
		return 0;
	}
}

//---------------------------------------------------------------------
// Bayer

inline constexpr int bayer8x8[8][8] = {
		{ 0,48,12,60,3,51,15,63 },
		{32,16,44,28,35,19,47,31},
		{ 8,56, 4,52,11,59, 7,55},
		{40,24,36,20,43,27,39,23},
		{ 2,50,14,62, 1,49,13,61},
		{34,18,46,30,33,17,45,29},
		{10,58, 6,54, 9,57, 5,53},
		{42,26,38,22,41,25,37,21}
};

inline constexpr int bayer4x4[4][4] = {
	{ 0, 8, 2,10 },
	{12, 4,14, 6 },
	{ 3,11, 1, 9 },
	{15, 7,13, 5 }
};

inline float get_bayer(BayerMode bayer, int x, int y) {
	switch (bayer) {
	case BayerMode::Bayer8x8:
		return (bayer8x8[y & 7][x & 7] / 64.0f) - 0.5f;
	case BayerMode::Bayer4x4:
		return (bayer4x4[y & 3][x & 3] / 16.0f) - 0.5f;
	}
	return 0;
}

//---------------------------------------------------------------------
// quantize

inline uint8_t quantize_index(
	const BayerDitherConfig& config,
	const unsigned char palette[][3],
	size_t count,
	int r, int g, int b,
	int x, int y)
{
	float d = get_bayer(config.bayer, x, y);
	int offset = (int)std::roundf(d * 64.0f * config.strength);

	r = std::clamp(r + offset, 0, 255);
	g = std::clamp(g + offset, 0, 255);
	b = std::clamp(b + offset, 0, 255);

	int best = 0;
	int best_dist = INT_MAX;

	for (int i = 0; i < count; i++) {
		int dr = r - palette[i][0];
		int dg = g - palette[i][1];
		int db = b - palette[i][2];

		int dist;
		if (config.hoge == 1) {
			dist = 3 * dr*dr + 6 * dg*dg + 1 * db*db; // にんげん向け. 緑の重みを強める.
		} else {
			dist = dr * dr + dg * dg + db * db;
		}

		if (dist < best_dist) {
			best_dist = dist;
			best = i;
		}
	}
	return (uint8_t)best;
}

inline void quantize(const BayerDitherConfig& config,
		int r, int g, int b,
		int x, int y,
		unsigned char& out_r, unsigned char& out_g, unsigned char& out_b) {

	const unsigned char (*palette)[3] = nullptr;
	auto size = get_palette(config, palette);
	auto best = quantize_index(config, palette, size, r, g, b, x, y);

	out_r = palette[best][0];
	out_g = palette[best][1];
	out_b = palette[best][2];
}

inline uint8_t quantize_index(const BayerDitherConfig& config,
															int r, int g, int b,
															int x, int y) {

	const unsigned char (*palette)[3] = nullptr;
	auto size = get_palette(config, palette);
	return quantize_index(config, palette, size, r, g, b, x, y);
}


inline void convert_to_indexed(const BayerDitherConfig& config, uint8_t* src, uint8_t* dst, int w, int h, int stride) {
	for (int y = 0; y < h; y++) {
		int yy = h - 1 - y; // BMP上下反転
		uint8_t* row = src + yy * stride;

		for (int x = 0; x < w; x++) {
			uint8_t* px = row + x * 3;

			uint8_t b = px[0];
			uint8_t g = px[1];
			uint8_t r = px[2];

			uint8_t idx = quantize_index(config, r, g, b, x, y);

			dst[y * w + x] = idx;
		}
	}
}

inline void convert_to_indexed(const BayerDitherConfig& config, uint8_t* src, uint8_t* dst, int w, int h) {
	int stride = w * 3;
	convert_to_indexed(config, src, dst, w, h, stride);
}

//------------------------------------------------------------------------------
