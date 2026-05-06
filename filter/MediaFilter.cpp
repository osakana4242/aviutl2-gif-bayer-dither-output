//----------------------------------------------------------------------------------
//	Bayer Dithering + Web Safe Color Filter for AviUtl ExEdit2
//----------------------------------------------------------------------------------

#define NOMINMAX

#include <algorithm>
#include <memory>
#include <windows.h>

#include "filter2.h"
#include "common.h"

bool func_proc_video(FILTER_PROC_VIDEO *video);

//---------------------------------------------------------------------
// 設定項目
//---------------------------------------------------------------------
FILTER_ITEM_SELECT::ITEM mode_list[(int)ColorMode::Count] = {};
auto mode = FILTER_ITEM_SELECT(L"カラーモード", (int)ColorMode::C16, mode_list);
static struct ModeListInitializer{
	ModeListInitializer() {
		for ( int i = 0; i < (int)ColorMode::Count; i++) {
			mode_list[i] = { g_mode_display_names[i], i };
		}
		mode_list[(int)ColorMode::Count] = { nullptr };
	}
} _mode_list_initializer;

auto strength = FILTER_ITEM_TRACK(L"ディザ強度", 2.0, 0.0, 2.0, 0.01);

FILTER_ITEM_SELECT::ITEM bayer_list[] = {
	{g_bayer_display_names[(int)BayerMode::Bayer8x8], (int)BayerMode::Bayer8x8},
	{g_bayer_display_names[(int)BayerMode::Bayer4x4], (int)BayerMode::Bayer4x4},
	{ nullptr }
};
auto bayer_mode = FILTER_ITEM_SELECT(L"ベイヤーサイズ", (int)BayerMode::Bayer4x4, bayer_list);

auto custom_color_count = FILTER_ITEM_TRACK(L"カスタムパレット色数", 16, 1, 256, 1);

auto hoge = FILTER_ITEM_TRACK(L"hoge", 0, 0, 1, 1);
auto color_shift = FILTER_ITEM_TRACK(L"shift", 0, 0, 8, 1);

void* items[] = { &mode, &bayer_mode, &custom_color_count, &color_shift, &hoge, nullptr };

//---------------------------------------------------------------------
// プラグイン定義
//---------------------------------------------------------------------
FILTER_PLUGIN_TABLE filter_plugin_table = {
	FILTER_PLUGIN_TABLE::FLAG_VIDEO,
	L"ベイヤーディザ", // ラベルの初期値 (nullptrならデフォルトのラベルになります)
	L"ベイヤーディザ", // プラグインの名前
	L"ベイヤーディザ v1.0.0 By 三川おさかな", // プラグインの情報
	items, // 画像フィルタ処理関数へのポインタ (FLAG_VIDEOが有効の時のみ呼ばれます)
	func_proc_video, // 設定項目の定義 (FILTER_ITEM_XXXポインタを列挙してnull終端したリストへのポインタ)
	nullptr // 音声フィルタ処理関数へのポインタ (FLAG_AUDIOが有効の時のみ呼ばれます)
};

//---------------------------------------------------------------------
//	プラグインDLL初期化関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) { // versionは本体のバージョン番号
	return true;
}

//---------------------------------------------------------------------
//	プラグインDLL解放関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void UninitializePlugin() {
}

//---------------------------------------------------------------------
//	フィルタ構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) FILTER_PLUGIN_TABLE* GetFilterPluginTable(void) {
	return &filter_plugin_table;
}


#include <unordered_map>
#include <cstdint>

std::vector<HistColor> collect_histogram(const BayerDitherConfig& config, PIXEL_RGBA* p, int w, int h) {

	const int pixel_step = 2;
	const int frame_step = 1;

	std::unordered_map<uint32_t, uint32_t> map;
	map.reserve(100000);

	int shift = config.color_shift;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			uint32_t key;
			uint8_t r = p->r;
			uint8_t g = p->g;
			uint8_t b = p->b;
			r >>= shift;
			g >>= shift;
			b >>= shift;

			key =
				(r << 16) |
				(g << 8) |
				b;

			map[key]++;
			p++;
		}
	}

	// vectorに変換
	std::vector<HistColor> hist;
	hist.reserve(map.size());

	for (auto&[k, cnt] : map) {
		hist.push_back({
				(uint8_t)(((uint8_t)(k >> 16)) << shift),
				(uint8_t)(((uint8_t)(k >> 8)) << shift),
				(uint8_t)(((uint8_t)(k)) << shift),
				cnt
									 });
	}

	return hist;
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
	BayerDitherConfig config;
	config.mode = (ColorMode)mode.value;
	config.bayer = (BayerMode)bayer_mode.value;
	config.strength = strength.value;
	config.color_count = custom_color_count.value;
	config.color_shift = color_shift.value;
	config.hoge = (int)hoge.value;


	if (config.mode == ColorMode::Custom) {
		auto samples = collect_histogram(config, p, w, h);
		auto palette = median_cut_histogram(samples, config.color_count);
		palette_custom_size = config.color_count;
		for (size_t i = 0; i < palette.size(); i++) {
			palette_custom[i][0] = palette[i].r;
			palette_custom[i][1] = palette[i].g;
			palette_custom[i][2] = palette[i].b;
		}
	}

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			unsigned char r, g, b;
			quantize(config, p->r, p->g, p->b, x, y, r, g, b);
			p->r = r;
			p->g = g;
			p->b = b;
			p++;
		}
	}

	video->set_image_data(buffer.get(), w, h);
	return true;
}
