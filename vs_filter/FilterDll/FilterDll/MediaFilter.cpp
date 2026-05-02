//----------------------------------------------------------------------------------
//	Bayer Dithering + Web Safe Color Filter for AviUtl ExEdit2
//----------------------------------------------------------------------------------
#include <algorithm>
#include <memory>
#include <windows.h>

#include "filter2.h"

bool func_proc_video(FILTER_PROC_VIDEO *video);

//---------------------------------------------------------------------
// 設定項目
//---------------------------------------------------------------------
FILTER_ITEM_SELECT::ITEM mode_list[] = {
	{L"Webセーフ(216色) ベイヤー8x8", 0},
	{L"Webセーフ(216色) ベイヤー4x4", 1},
	{L"VGA 16色 ベイヤー8x8", 2},
	{L"VGA 16色 ベイヤー4x4", 3},
	{nullptr}
};
auto mode = FILTER_ITEM_SELECT(L"カラーモード", 0, mode_list);
auto strength = FILTER_ITEM_TRACK(L"ディザ強度", 1.0, 0.0, 2.0, 0.01);
void *items[] = { &mode, &strength, nullptr };

//---------------------------------------------------------------------
// プラグイン定義
//---------------------------------------------------------------------
FILTER_PLUGIN_TABLE filter_plugin_table = {
	FILTER_PLUGIN_TABLE::FLAG_VIDEO,
	L"ベイヤーディザ減色",                // プラグインの名前
	L"ベイヤーディザ減色",                // ラベルの初期値 (nullptrならデフォルトのラベルになります)
	L"ベイヤーディザ減色 by osakana4242", // プラグインの情報
	items,                                // 画像フィルタ処理関数へのポインタ (FLAG_VIDEOが有効の時のみ呼ばれます)
	func_proc_video,                      // 設定項目の定義 (FILTER_ITEM_XXXポインタを列挙してnull終端したリストへのポインタ)
	nullptr                               // 音声フィルタ処理関数へのポインタ (FLAG_AUDIOが有効の時のみ呼ばれます)
};

EXTERN_C __declspec(dllexport) FILTER_PLUGIN_TABLE *GetFilterPluginTable(void) {
	return &filter_plugin_table;
}

// パレットVGA
static const unsigned char palette16[16][3] = {
	{0, 0, 0},       {128, 0, 0},   {0, 128, 0},   {128, 128, 0},
	{0, 0, 128},     {128, 0, 128}, {0, 128, 128}, {192, 192, 192},
	{128, 128, 128}, {255, 0, 0},   {0, 255, 0},   {255, 255, 0},
	{0, 0, 255},     {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
};

//---------------------------------------------------------------------
// Bayer 8x8 行列 (0～63)
//---------------------------------------------------------------------
static const int bayer8x8[8][8] = {
	{0, 48, 12, 60, 3, 51, 15, 63}, {32, 16, 44, 28, 35, 19, 47, 31},
	{8, 56, 4, 52, 11, 59, 7, 55},  {40, 24, 36, 20, 43, 27, 39, 23},
	{2, 50, 14, 62, 1, 49, 13, 61}, {34, 18, 46, 30, 33, 17, 45, 29},
	{10, 58, 6, 54, 9, 57, 5, 53},  {42, 26, 38, 22, 41, 25, 37, 21}
};

//---------------------------------------------------------------------
// Webセーフ量子化 (ディザ込み)
//---------------------------------------------------------------------
inline unsigned char quantize_websafe(int value, int x, int y,
									  double strength_val) {
	// Bayer値を -0.5 ～ +0.5 に正規化
	double d = (bayer8x8[y & 7][x & 7] / 64.0) - 0.5;

	// ディザ適用
	double v = value + d * 51.0 * strength_val;

	// 0～255 clamp
	v = std::clamp(v, 0.0, 255.0);

	// 0～5 に量子化
	int level = (int)(v / 51.0 + 0.5);
	level = std::clamp(level, 0, 5);

	// 0,51,102,...255 に戻す
	return (unsigned char)(level * 51);
}

//---------------------------------------------------------------------
// VGA16量子化 (ディザ込み)
//---------------------------------------------------------------------

inline int color_dist(int r1, int g1, int b1, int r2, int g2, int b2) {
	int dr = r1 - r2;
	int dg = g1 - g2;
	int db = b1 - b2;
	//	return dr * dr + dg * dg + db * db;
	return dr * dr * 3 + dg * dg * 6 + db * db * 1;
}

inline void quantize_vga16(int r, int g, int b, int x, int y, double strength,
						   unsigned char &out_r, unsigned char &out_g,
						   unsigned char &out_b) {
	double d = (bayer8x8[y & 7][x & 7] / 64.0) - 0.5;

	r = (int)(r + d * 64.0 * strength);
	g = (int)(g + d * 64.0 * strength);
	b = (int)(b + d * 64.0 * strength);

	r = std::clamp(r, 0, 255);
	g = std::clamp(g, 0, 255);
	b = std::clamp(b, 0, 255);

	int best = 0;
	int best_dist = INT_MAX;

	for (int i = 0; i < 16; i++) {
		int pr = palette16[i][0];
		int pg = palette16[i][1];
		int pb = palette16[i][2];

		int dist = color_dist(r, g, b, pr, pg, pb);
		if (dist < best_dist) {
			best_dist = dist;
			best = i;
		}
	}

	out_r = palette16[best][0];
	out_g = palette16[best][1];
	out_b = palette16[best][2];
}

//---------------------------------------------------------------------
// 画像処理
//---------------------------------------------------------------------
bool func_proc_video(FILTER_PROC_VIDEO *video) {
	auto w = video->object->width;
	auto h = video->object->height;

	auto buffer = std::make_unique<PIXEL_RGBA[]>(w * h);
	video->get_image_data(buffer.get());

	auto p = buffer.get();
	double s = strength.value;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {

			if (mode.value == 0) {
				// Webセーフ
				p->r = quantize_websafe(p->r, x, y, s);
				p->g = quantize_websafe(p->g, x, y, s);
				p->b = quantize_websafe(p->b, x, y, s);
			}
			else {
				// VGA16
				unsigned char r, g, b;
				quantize_vga16(p->r, p->g, p->b, x, y, s, r, g, b);
				p->r = r;
				p->g = g;
				p->b = b;
			}

			p++;
		}
	}

	video->set_image_data(buffer.get(), w, h);
	return true;
}
