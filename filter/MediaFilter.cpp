//----------------------------------------------------------------------------------
//	Bayer Dithering + Web Safe Color Filter for AviUtl ExEdit2
//----------------------------------------------------------------------------------
#include <algorithm>
#include <memory>
#include <windows.h>

#include "filter2.h"
#include "common.h"

bool func_proc_video(FILTER_PROC_VIDEO *video);

//---------------------------------------------------------------------
// 設定項目
//---------------------------------------------------------------------
FILTER_ITEM_SELECT::ITEM mode_list[] = {
	{g_mode_display_names[(int)ColorMode::C256], (int)ColorMode::C256},
	{g_mode_display_names[(int)ColorMode::C16], (int)ColorMode::C16},
	{g_mode_display_names[(int)ColorMode::C8], (int)ColorMode::C8},
	{nullptr}
};
auto mode = FILTER_ITEM_SELECT(L"カラーモード", (int)ColorMode::C16, mode_list);
auto strength = FILTER_ITEM_TRACK(L"ディザ強度", 2.0, 0.0, 2.0, 0.01);

FILTER_ITEM_SELECT::ITEM bayer_list[] = {
	{g_bayer_display_names[(int)BayerMode::Bayer8x8], (int)BayerMode::Bayer8x8},
	{g_bayer_display_names[(int)BayerMode::Bayer4x4], (int)BayerMode::Bayer4x4},
	{ nullptr }
};
auto bayer_mode = FILTER_ITEM_SELECT(L"ベイヤーサイズ", (int)BayerMode::Bayer4x4, bayer_list);

void* items[] = { &mode, &bayer_mode, &strength, nullptr };

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
