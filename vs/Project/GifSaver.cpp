//----------------------------------------------------------------------------------
//	ベイヤーディザ減色GIF出力プラグイン for AviUtl ExEdit2
//----------------------------------------------------------------------------------

#include <string>
#include <algorithm>
#include <vector>
#include <windows.h>
#include <vfw.h>
#include <commctrl.h>
#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "comctl32.lib")
#include "gif_lib.h"
#include "resource.h"
#include "output2.h"


#include <cstdio>
#include <cstdarg>
#include <cstdlib>

std::string wide_to_utf8(const wchar_t* w) {
	int size = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
	std::string s(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), size, NULL, NULL);
	return s;
}

bool func_output(OUTPUT_INFO* oip);
bool func_config(HWND hwnd, HINSTANCE dll_hinst);
LPCWSTR func_get_config_text();

//---------------------------------------------------------------------
//	出力プラグイン構造体定義
//---------------------------------------------------------------------
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	OUTPUT_PLUGIN_TABLE::FLAG_VIDEO | OUTPUT_PLUGIN_TABLE::FLAG_AUDIO, //	フラグ
	L"ベイヤーディザ減色GIF出力", // プラグインの名前
	L"GifFile (*.gif)\0*.gif\0", // 出力ファイルのフィルタ
	L"ベイヤーディザ減色GIF出力 v1.0.0 By 三川おさかな", // プラグインの情報
	func_output, // 出力時に呼ばれる関数へのポインタ
	func_config, // 出力設定のダイアログを要求された時に呼ばれる関数へのポインタ (nullptrなら呼ばれません)
	func_get_config_text, // 出力設定のテキスト情報を取得する時に呼ばれる関数へのポインタ (nullptrなら呼ばれません)
};

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
// 設定構造体
//---------------------------------------------------------------------
struct CONFIG {
	ColorMode mode = ColorMode::C8;
	BayerMode bayer = BayerMode::Bayer4x4;
	float strength = 2.0;
};

static CONFIG g_config;

//---------------------------------------------------------------------
// パレット

// パレット8
static const unsigned char palette_8[8][3] = {
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
static const unsigned char palette_16[16][3] = {
	{0, 0, 0},       {128, 0, 0},   {0, 128, 0},   {128, 128, 0},
	{0, 0, 128},     {128, 0, 128}, {0, 128, 128}, {192, 192, 192},
	{128, 128, 128}, {255, 0, 0},   {0, 255, 0},   {255, 255, 0},
	{0, 0, 255},     {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
};

// パレット256
static unsigned char palette_256[256][3] = {};

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

//---------------------------------------------------------------------
// Bayer

static const int bayer8x8[8][8] = {
		{ 0,48,12,60,3,51,15,63 },
		{32,16,44,28,35,19,47,31},
		{ 8,56, 4,52,11,59, 7,55},
		{40,24,36,20,43,27,39,23},
		{ 2,50,14,62, 1,49,13,61},
		{34,18,46,30,33,17,45,29},
		{10,58, 6,54, 9,57, 5,53},
		{42,26,38,22,41,25,37,21}
};

static const int bayer4x4[4][4] = {
	{ 0, 8, 2,10 },
	{12, 4,14, 6 },
	{ 3,11, 1, 9 },
	{15, 7,13, 5 }
};

inline float get_bayer(int x, int y) {
	switch (g_config.bayer) {
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
	int x, int y)
{
	float d = get_bayer(x, y);
	int offset = (int)std::roundf(d * 64.0f * g_config.strength);

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
	int x, int y,
	unsigned char& out_r, unsigned char& out_g, unsigned char& out_b) {

	auto best = quantize_index(palette, r, g, b, x, y);

	out_r = palette[best][0];
	out_g = palette[best][1];
	out_b = palette[best][2];
}

inline uint8_t quantize_index(int r, int g, int b, int x, int y) {
	switch (g_config.mode) {
	case ColorMode::C256:
		return quantize_index(palette_256, r, g, b, x, y);
	case ColorMode::C16:
		return quantize_index(palette_16, r, g, b, x, y);
	case ColorMode::C8:
		return quantize_index(palette_8, r, g, b, x, y);
	default:
		return 0;
	}
}


void convert_to_indexed(uint8_t* src, uint8_t* dst, int w, int h) {
	int stride = w * 3;

	for (int y = 0; y < h; y++) {
		int yy = h - 1 - y; // BMP上下反転
		uint8_t* row = src + yy * stride;

		for (int x = 0; x < w; x++) {
			uint8_t* px = row + x * 3;

			uint8_t b = px[0];
			uint8_t g = px[1];
			uint8_t r = px[2];

			uint8_t idx = quantize_index(r, g, b, x, y);

			dst[y * w + x] = idx;
		}
	}
}

//---------------------------------------------------------------------

template<size_t N>
ColorMapObject* create_palette(const unsigned char (&palette)[N][3]) {
	auto* map = GifMakeMapObject(N, NULL);

	for (int i = 0; i < N; i++) {
		map->Colors[i].Red =   palette[i][0];
		map->Colors[i].Green = palette[i][1];
		map->Colors[i].Blue =  palette[i][2];
	}
	return map;
}

ColorMapObject* create_palette() {
	switch (g_config.mode) {
	case ColorMode::C256:
	fatal_error("unti うんち");
		return create_palette(palette_256);
	case ColorMode::C16:
		return create_palette(palette_16);
	case ColorMode::C8:
		return create_palette(palette_8);
	default:
		return nullptr;
	}
}

//---------------------------------------------------------------------
//	プラグインDLL初期化関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) { // versionは本体のバージョン番号
	init_palette_websafe();
	return true;
}

//---------------------------------------------------------------------
//	プラグインDLL終了関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void UninitializePlugin() {
}

//---------------------------------------------------------------------
//	出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) OUTPUT_PLUGIN_TABLE* GetOutputPluginTable(void) {
	return &output_plugin_table;
}

//---------------------------------------------------------------------
//	AVIファイルハンドル構造体
//---------------------------------------------------------------------
struct AVI_HANDLE {
	PAVIFILE	pfile = nullptr;
	PAVISTREAM	pvideo = nullptr;
	PAVISTREAM	paudio = nullptr;
	~AVI_HANDLE() {
		if (paudio) AVIStreamRelease(paudio);
		if (pvideo) AVIStreamRelease(pvideo);
		if (pfile) AVIFileRelease(pfile);
	}
};

//---------------------------------------------------------------------
//	出力プラグイン出力関数
//---------------------------------------------------------------------
bool func_output(OUTPUT_INFO* oip) {

	int error = 0;
	std::string path = wide_to_utf8(oip->savefile);
	GifFileType* gif = EGifOpenFileName(path.c_str(), false, &error);
	if (!gif) return false;

	auto* palette = create_palette();

	EGifPutScreenDesc(gif, oip->w, oip->h, 8, 0, palette);

	std::vector<uint8_t> indexed(oip->w * oip->h);

	float fps = (float)oip->rate / oip->scale;
	int delay = (int)(100.0 / fps);

	// ループ設定（無限）
	{
		unsigned char loop[] = {1, 0, 0};
		EGifPutExtensionLeader(gif, APPLICATION_EXT_FUNC_CODE);
		EGifPutExtensionBlock(gif, 11, "NETSCAPE2.0");
		EGifPutExtensionBlock(gif, 3, loop);
		EGifPutExtensionTrailer(gif);
	}

	for (int frame = 0; frame < oip->n; frame++) {

		oip->func_rest_time_disp(frame, oip->n);
		if (oip->func_is_abort()) break;

		uint8_t* src = (uint8_t*)oip->func_get_video(frame, BI_RGB);

		convert_to_indexed(src, indexed.data(), oip->w, oip->h);

		// 遅延
		unsigned char gce[4];
		gce[0] = 0x04;
		gce[1] = delay & 0xFF;
		gce[2] = (delay >> 8) & 0xFF;
		gce[3] = 0;

		EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, 4, gce);

		EGifPutImageDesc(gif, 0, 0, oip->w, oip->h, false, NULL);

		for (int y = 0; y < oip->h; y++) {
			EGifPutLine(gif, indexed.data() + y * oip->w, oip->w);
		}
	}

	EGifCloseFile(gif, &error);
	return true;
}

//---------------------------------------------------------------------
//	設定ダイアログ
//---------------------------------------------------------------------
INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {

	case WM_INITDIALOG:
	{

		// モード
		HWND hMode = GetDlgItem(hDlg, IDC_MODE);
		for (int i = 0; i < (int)ColorMode::Count; i++) {
			SendMessageW(hMode, CB_ADDSTRING, 0,
				(LPARAM)g_mode_display_names[i]);
		}
		SendMessageW(hMode, CB_SETCURSEL, (int)g_config.mode, 0);

		// Bayer
		HWND hBayer = GetDlgItem(hDlg, IDC_BAYER);
		for (int i = 0; i < (int)BayerMode::Count; i++) {
			SendMessageW(hBayer, CB_ADDSTRING, 0,
				(LPARAM)g_bayer_display_names[i]);
		}
		SendMessageW(hBayer, CB_SETCURSEL, (int)g_config.bayer, 0);

		// スライダー
		HWND slider = GetDlgItem(hDlg, IDC_STRENGTH);
		SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200)); // 0.0〜2.0
		SendMessageW(slider, TBM_SETPOS, TRUE, (LPARAM)(g_config.strength * 100));
		SendMessageW(slider, TBM_SETTICFREQ, 10, 0); // 0.1刻みの目盛り

		wchar_t buf[32];
		swprintf_s(buf, L"%.2f", g_config.strength);
		SetDlgItemTextW(hDlg, IDC_STRENGTH_TEXT, buf);

		return TRUE;
	}

	case WM_HSCROLL:
	{
		HWND slider = GetDlgItem(hDlg, IDC_STRENGTH);
		int pos = (int)SendMessageW(slider, TBM_GETPOS, 0, 0);

		float val = pos / 100.0f;

		wchar_t buf[32];
		swprintf_s(buf, L"%.2f", val);
		SetDlgItemTextW(hDlg, IDC_STRENGTH_TEXT, buf);

		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDOK:
		{

			// モード
			g_config.mode = (ColorMode)SendMessageW(
					GetDlgItem(hDlg, IDC_MODE), CB_GETCURSEL, 0, 0);

			// Bayer
			g_config.bayer = (BayerMode)SendMessageW(
					GetDlgItem(hDlg, IDC_BAYER), CB_GETCURSEL, 0, 0);

			// 強度
			HWND slider = GetDlgItem(hDlg, IDC_STRENGTH);
			int pos = (int)SendMessageW(slider, TBM_GETPOS, 0, 0);
			g_config.strength = pos / 100.0;

			EndDialog(hDlg, IDOK);
			return TRUE;
		}

		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

bool func_config(HWND hwnd, HINSTANCE dll_hinst) {

	// コモンコントロール初期化（重要）
	INITCOMMONCONTROLSEX ic{};
	ic.dwSize = sizeof(ic);
	ic.dwICC = ICC_BAR_CLASSES;
	InitCommonControlsEx(&ic);

	INT_PTR ret = DialogBox(
		dll_hinst,
		MAKEINTRESOURCE(IDD_CONFIG),
		hwnd,
		ConfigDlgProc
	);

	return (ret == IDOK);
}


//---------------------------------------------------------------------
//	出力設定のテキスト情報 (設定ボタンの隣に表示される)
//---------------------------------------------------------------------
// 文字列表示（UI右側に出るやつ）
LPCWSTR func_get_config_text() {
	static wchar_t buf[256];

	const wchar_t* mode = g_mode_display_names[(int)g_config.mode];

	const wchar_t* bayer =g_bayer_display_names[(int)g_config.bayer];

	swprintf_s(buf, L"%s / Bayer %s / 強度 %.2f",
						 mode, bayer, g_config.strength);

	return buf;
}

