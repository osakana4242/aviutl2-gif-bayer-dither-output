#pragma once

#include <string>
#include <iterator>
#include "logger2.h"
#include "median_cut.h"

//---------------------------------------------------------------------
// Logger
//---------------------------------------------------------------------

LOG_HANDLE* g_logger;

EXTERN_C __declspec(dllexport) void InitializeLogger(LOG_HANDLE* logger) {
	g_logger = logger;
}

//---------------------------------------------------------------------
// ColoMode
//---------------------------------------------------------------------

enum class ColorMode {
	Custom,
	Select16,
	Select256,
	C2,
	C4,
	C8,
	C16,
	C216,
	C256,
	Count,
};

struct ColorModeModel {
	ColorMode id;
	const wchar_t* name;
	const unsigned char (*palette)[3];
	int palette_size;
	int color_shift;
};

//---------------------------------------------------------------------
// BayerMode
//---------------------------------------------------------------------

enum class BayerMode {
	Bayer8x8 = 0,
	Bayer4x4 = 1,
	Count,
};

static const wchar_t* g_bayer_display_names[] = {
	L"8x8",
	L"4x4",
};

static_assert((int)BayerMode::Count == _countof(g_bayer_display_names),
	"bayer name mismatch");

//---------------------------------------------------------------------

static const wchar_t* g_color_shift_display_names[] = {
	L"256",
	L"128",
	L"64",
	L"32",
	L"16",
	L"8",
	L"4",
	L"2",
};

//---------------------------------------------------------------------

struct BayerDitherConfig {
	ColorMode color_mode = ColorMode::Select16;
	BayerMode bayer = BayerMode::Bayer4x4;
	float bayer_strength = 2.0;
	int custom_palette_size = 16;
	int custom_color_shift = 3; // RGB各要素に適用する量子化シフト数. 0: 256段階, 3: 32段階(32768色), 4: 16段階(4096色), 5: 8段階(512色)
	int perceptual_color_diff = 1; // 知覚色差補正. 0: OFF, 1: ON
};

//---------------------------------------------------------------------
// パレット

// パレット2
inline constexpr unsigned char palette_2[2][3] = {
	{0, 0, 0},
	{170, 170, 170}
};

// パレット4
inline constexpr unsigned char palette_4[4][3] = {
	{0, 0, 0},
	{0, 170, 170},
	{170, 0, 170},
	{170, 170, 170}
};

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
inline unsigned char g_palette_custom[256][3] = {};

struct PaletteInitializer {
	PaletteInitializer() {
		init_palette_216();
		init_palette_256();
	}
};
inline PaletteInitializer g_palette_initializer;

const ColorModeModel g_color_mode_models[] = {
	{ColorMode::Custom , L"カスタム", nullptr, 0, 0},
	{ColorMode::Select16, L"選択16色", nullptr, 16, 3},
	{ColorMode::Select256, L"選択256色", nullptr, 256, 3},
	{ColorMode::C8, L"VGA8色", palette_8, std::size(palette_8), 0},
	{ColorMode::C16, L"VGA16色", palette_16, std::size(palette_16), 0},
	{ColorMode::C216, L"Webセーフ216色", palette_216, std::size(palette_216), 0},
};

const ColorModeModel* get_color_model(ColorMode id) {
	for (int i = 0, iCount = std::size(g_color_mode_models); i < iCount; i++) {
		auto& item = g_color_mode_models[i];
		if (item.id == id) {
			wchar_t buf[256];
			wsprintf(buf, L"hit %d", id);
			g_logger->log(g_logger, buf);
			return &item;
		}
	}
	{
		wchar_t buf[256];
		wsprintf(buf, L"no hit. size: %d", std::size(g_color_mode_models));
		g_logger->log(g_logger, buf);
	}
	return nullptr;
}

inline size_t get_palette(const BayerDitherConfig& config, const unsigned char (*&palette)[3]) {
	auto model = get_color_model(config.color_mode);
	if (model->palette != nullptr) {
		palette = model->palette;
		return model->palette_size;
	}
	palette = g_palette_custom;
	if (model->palette_size != 0) {
		return model->palette_size;
	}
	return config.custom_palette_size;
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
	int offset = (int)std::roundf(d * 64.0f * config.bayer_strength);

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
		if (config.perceptual_color_diff == 1) {
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
