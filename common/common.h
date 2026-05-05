#pragma once

#include <string>


//---------------------------------------------------------------------
// ColoMode
//---------------------------------------------------------------------

enum class ColorMode {
	C256 = 0,
	C16 = 1,
	C8 = 2,
	Count,
};

static const wchar_t* g_mode_display_names[] = {
	L"パレット256色",
	L"パレット16色",
	L"パレット8色"
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
	{0, 0, 0},       {128, 0, 0},   {0, 128, 0},   {128, 128, 0},
	{0, 0, 128},     {128, 0, 128}, {0, 128, 128}, {192, 192, 192},
	{128, 128, 128}, {255, 0, 0},   {0, 255, 0},   {255, 255, 0},
	{0, 0, 255},     {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
};

// パレット256
inline unsigned char palette_256[256][3] = {};

inline void init_palette_websafe() {
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

struct PaletteInitializer {
    PaletteInitializer() { init_palette_websafe(); }
};
inline PaletteInitializer g_palette_initializer;

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

template<size_t N>
inline uint8_t quantize_index(
	const unsigned char (&palette)[N][3],
	int r, int g, int b,
	int x, int y,
	BayerMode bayer, float strength)
{
	float d = get_bayer(bayer, x, y);
	int offset = (int)std::roundf(d * 64.0f * strength);

	r = std::clamp(r + offset, 0, 255);
	g = std::clamp(g + offset, 0, 255);
	b = std::clamp(b + offset, 0, 255);

	int best = 0;
	int best_dist = INT_MAX;

	for (int i = 0; i < N; i++) {
		int dr = r - palette[i][0];
		int dg = g - palette[i][1];
		int db = b - palette[i][2];
		// int dist = dr*dr + dg*dg + db*db;
		int dist = 3*dr*dr + 6*dg*dg + 1*db*db; // にんげん向け. 緑の重みを強める.

		if (dist < best_dist) {
			best_dist = dist;
			best = i;
		}
	}
	return (uint8_t)best;
}

template<size_t N>
inline void quantize(
	const unsigned char (&palette)[N][3],
	int r, int g, int b,
	int x, int y, BayerMode bayer, float strength,
	unsigned char& out_r, unsigned char& out_g, unsigned char& out_b) {

	auto best = quantize_index(palette, r, g, b, x, y, bayer, strength);

	out_r = palette[best][0];
	out_g = palette[best][1];
	out_b = palette[best][2];
}


inline void quantize(ColorMode mode,
		int r, int g, int b,
		int x, int y, BayerMode bayer, float strength,
		unsigned char& out_r, unsigned char& out_g, unsigned char& out_b) {

	switch (mode) {
	case ColorMode::C256:
		quantize(palette_256, r, g, b, x, y, bayer, strength, out_r, out_g, out_b);
		break;
	case ColorMode::C16:
		quantize(palette_16, r, g, b, x, y, bayer, strength, out_r, out_g, out_b);
		break;
	case ColorMode::C8:
		quantize(palette_8, r, g, b, x, y, bayer, strength, out_r, out_g, out_b);
		break;
	default:
		break;
	}
}



inline uint8_t quantize_index(ColorMode mode,
															int r, int g, int b,
															int x, int y, BayerMode bayer, float strength) {
	switch (mode) {
	case ColorMode::C256:
		return quantize_index(palette_256, r, g, b, x, y, bayer, strength);
	case ColorMode::C16:
		return quantize_index(palette_16, r, g, b, x, y, bayer, strength);
	case ColorMode::C8:
		return quantize_index(palette_8, r, g, b, x, y, bayer, strength);
	default:
		return 0;
	}
}


inline void convert_to_indexed(uint8_t* src, uint8_t* dst, int w, int h, int stride, ColorMode mode, BayerMode bayer, float strength) {
	for (int y = 0; y < h; y++) {
		int yy = h - 1 - y; // BMP上下反転
		uint8_t* row = src + yy * stride;

		for (int x = 0; x < w; x++) {
			uint8_t* px = row + x * 3;

			uint8_t b = px[0];
			uint8_t g = px[1];
			uint8_t r = px[2];

			uint8_t idx = quantize_index(mode, r, g, b, x, y, bayer, strength);

			dst[y * w + x] = idx;
		}
	}
}

inline void convert_to_indexed(uint8_t* src, uint8_t* dst, int w, int h, ColorMode mode, BayerMode bayer, float strength) {
	int stride = w * 3;
	convert_to_indexed(src, dst, w, h, stride, mode, bayer, strength);
}

