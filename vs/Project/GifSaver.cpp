//----------------------------------------------------------------------------------
//	サンプルAVI(vfw経由)出力プラグイン for AviUtl ExEdit2
//----------------------------------------------------------------------------------

#include <string>
#include <algorithm>
#include <vector>
#include <windows.h>
#include <vfw.h>
#pragma comment(lib, "vfw32.lib")

#include "output2.h"

#include "gif_lib.h"

// for DialogProc
#include <commctrl.h>
#include "resource.h"
#pragma comment(lib, "comctl32.lib")
//


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
	L"GIF File Saver osakana4242",					// プラグインの名前
	L"GifFile (*.gif)\0*.gif\0",					// 出力ファイルのフィルタ
	L"GIF File Saver version 1.00 By 三川おさかな",	// プラグインの情報
	func_output,									// 出力時に呼ばれる関数へのポインタ
	func_config,									// 出力設定のダイアログを要求された時に呼ばれる関数へのポインタ (nullptrなら呼ばれません)
	func_get_config_text,							// 出力設定のテキスト情報を取得する時に呼ばれる関数へのポインタ (nullptrなら呼ばれません)
};

// 設定構造体
struct CONFIG {
	int mode = 0;        // 0=WebSafe, 1=VGA16
	int bayer = 0;       // 0=8x8, 1=4x4
	double strength = 1.0;
};

static CONFIG g_config;

// パレットVGA
static const unsigned char palette16[16][3] = {
	{0, 0, 0},       {128, 0, 0},   {0, 128, 0},   {128, 128, 0},
	{0, 0, 128},     {128, 0, 128}, {0, 128, 128}, {192, 192, 192},
	{128, 128, 128}, {255, 0, 0},   {0, 255, 0},   {255, 255, 0},
	{0, 0, 255},     {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
};

// 8x8 Bayer
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

// 4x4
static const int bayer4x4[4][4] = {
	{ 0, 8, 2,10 },
	{12, 4,14, 6 },
	{ 3,11, 1, 9 },
	{15, 7,13, 5 }
};


inline double get_bayer(int x, int y) {
	if (g_config.bayer == 0) {
		return (bayer8x8[y & 7][x & 7] / 64.0) - 0.5;
	} else {
		return (bayer4x4[y & 3][x & 3] / 16.0) - 0.5;
	}
}

inline unsigned char quantize_websafe(int v, int x, int y) {
	double d = get_bayer(x, y);
	double val = v + d * 51.0 * g_config.strength;

	val = std::clamp(val, 0.0, 255.0);

	int level = (int)(val / 51.0 + 0.5);
	level = std::clamp(level, 0, 5);

	return (unsigned char)(level * 51);
}


inline void quantize_vga16(
	int r, int g, int b,
	int x, int y,
	unsigned char& or_, unsigned char& og, unsigned char& ob) {
	double d = get_bayer(x, y);

	r = std::clamp((int)(r + d * 64.0 * g_config.strength), 0, 255);
	g = std::clamp((int)(g + d * 64.0 * g_config.strength), 0, 255);
	b = std::clamp((int)(b + d * 64.0 * g_config.strength), 0, 255);

	int best = 0;
	int best_dist = INT_MAX;

	for (int i = 0; i < 16; i++) {
		int pr = palette16[i][0];
		int pg = palette16[i][1];
		int pb = palette16[i][2];

		int dr = r - pr;
		int dg = g - pg;
		int db = b - pb;

		int dist = dr * dr + dg * dg + db * db;

		if (dist < best_dist) {
			best_dist = dist;
			best = i;
		}
	}

	or_ = palette16[best][0];
	og = palette16[best][1];
	ob = palette16[best][2];
}

inline uint8_t quantize_websafe_index(int r, int g, int b, int x, int y) {
	double d = get_bayer(x, y);

	r = std::clamp((int)(r + d * 51.0 * g_config.strength), 0, 255);
	g = std::clamp((int)(g + d * 51.0 * g_config.strength), 0, 255);
	b = std::clamp((int)(b + d * 51.0 * g_config.strength), 0, 255);

	int ri = (r + 25) / 51;
	int gi = (g + 25) / 51;
	int bi = (b + 25) / 51;

	return (uint8_t)(ri * 36 + gi * 6 + bi);
}

inline uint8_t quantize_vga16_index(int r, int g, int b, int x, int y) {
	double d = get_bayer(x, y);

	r = std::clamp((int)(r + d * 64.0 * g_config.strength), 0, 255);
	g = std::clamp((int)(g + d * 64.0 * g_config.strength), 0, 255);
	b = std::clamp((int)(b + d * 64.0 * g_config.strength), 0, 255);

	int best = 0;
	int best_dist = INT_MAX;

	for (int i = 0; i < 16; i++) {
		int dr = r - palette16[i][0];
		int dg = g - palette16[i][1];
		int db = b - palette16[i][2];
		int dist = dr*dr + dg*dg + db*db;

		if (dist < best_dist) {
			best_dist = dist;
			best = i;
		}
	}
	return (uint8_t)best;
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

			uint8_t idx;

			if (g_config.mode == 0) {
				idx = quantize_websafe_index(r, g, b, x, y);
			} else {
				idx = quantize_vga16_index(r, g, b, x, y);
			}

			dst[y * w + x] = idx;
		}
	}
}


ColorMapObject* create_palette() {
	if (g_config.mode == 0) {
		auto* map = GifMakeMapObject(256, NULL);

		for (int r = 0; r < 6; r++)
		for (int g = 0; g < 6; g++)
		for (int b = 0; b < 6; b++) {
			int i = r*36 + g*6 + b;
			map->Colors[i].Red   = r * 51;
			map->Colors[i].Green = g * 51;
			map->Colors[i].Blue  = b * 51;
		}

		return map;
	}
	else {
		auto* map = GifMakeMapObject(16, NULL);

		for (int i = 0; i < 16; i++) {
			map->Colors[i].Red   = palette16[i][0];
			map->Colors[i].Green = palette16[i][1];
			map->Colors[i].Blue  = palette16[i][2];
		}
		return map;
	}
}

//---------------------------------------------------------------------
//	プラグインDLL初期化関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) { // versionは本体のバージョン番号
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

	double fps = (double)oip->rate / oip->scale;
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
		SendMessageW(hMode, CB_ADDSTRING, 0, (LPARAM)L"WebSafe(216)");
		SendMessageW(hMode, CB_ADDSTRING, 0, (LPARAM)L"VGA16");
		SendMessageW(hMode, CB_SETCURSEL, 0, 0);

		// Bayer
		HWND hBayer = GetDlgItem(hDlg, IDC_BAYER);
		SendMessageW(hBayer, CB_ADDSTRING, 0, (LPARAM)L"Bayer 8x8");
		SendMessageW(hBayer, CB_ADDSTRING, 0, (LPARAM)L"Bayer 4x4");
		SendMessageW(hBayer, CB_SETCURSEL, 0, 0);

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

		double val = pos / 100.0;

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
			g_config.mode = SendMessageW(GetDlgItem(hDlg, IDC_MODE), CB_GETCURSEL, 0, 0);
			// Bayer
			g_config.bayer = SendMessageW(GetDlgItem(hDlg, IDC_BAYER), CB_GETCURSEL, 0, 0);

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

	const wchar_t* mode =
		(g_config.mode == 0) ? L"WebSafe" : L"VGA16";

	const wchar_t* bayer =
		(g_config.bayer == 0) ? L"8x8" : L"4x4";

	swprintf_s(buf, L"%s / Bayer %s / 強度 %.2f",
						 mode, bayer, g_config.strength);

	return buf;
}

