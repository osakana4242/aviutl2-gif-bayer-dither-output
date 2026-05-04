//----------------------------------------------------------------------------------
//	サンプルAVI(vfw経由)出力プラグイン for AviUtl ExEdit2
//----------------------------------------------------------------------------------

#include <algorithm>
#include <windows.h>
#include <vfw.h>
#pragma comment(lib, "vfw32.lib")

#include "output2.h"

// for DialogProc
#include <commctrl.h>
#include "resource.h"
#pragma comment(lib, "comctl32.lib")
//

bool func_output(OUTPUT_INFO* oip);
bool func_config(HWND hwnd, HINSTANCE dll_hinst);
LPCWSTR func_get_config_text();

//---------------------------------------------------------------------
//	出力プラグイン構造体定義
//---------------------------------------------------------------------
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	OUTPUT_PLUGIN_TABLE::FLAG_VIDEO | OUTPUT_PLUGIN_TABLE::FLAG_AUDIO, //	フラグ
	L"AVI File Saver2 (sample)(osakana)ほげ",					// プラグインの名前
	L"AviFile (*.avi)\0*.avi\0",					// 出力ファイルのフィルタ
	L"Sample AVI File Saver2 version 2.01 By ＫＥＮくん",	// プラグインの情報
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


void process_frame(uint8_t* data, int w, int h) {
	// BI_RGB は下から上に並んでる
	int stride = w * 3;

	for (int y = 0; y < h; y++) {
		int yy = h - 1 - y; // 上下反転
		uint8_t* row = data + yy * stride;

		for (int x = 0; x < w; x++) {
			uint8_t* px = row + x * 3;

			// BGR順
			uint8_t b = px[0];
			uint8_t g = px[1];
			uint8_t r = px[2];

			if (g_config.mode == 0) {
				r = quantize_websafe(r, x, y);
				g = quantize_websafe(g, x, y);
				b = quantize_websafe(b, x, y);
			} else {
				quantize_vga16(r, g, b, x, y, r, g, b);
			}

			px[0] = b;
			px[1] = g;
			px[2] = r;
		}
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
	AVI_HANDLE avi;
	if (AVIFileOpen(&avi.pfile, oip->savefile, OF_WRITE | OF_CREATE, NULL) != S_OK) {
		return false;
	}

	// ビデオストリームの設定
	AVISTREAMINFO video{};
	video.fccType = streamtypeVIDEO;
	video.fccHandler = BI_RGB;
	video.dwRate = oip->rate;
	video.dwScale = oip->scale;
	video.rcFrame.right = oip->w;
	video.rcFrame.bottom = oip->h;
	if (AVIFileCreateStream(avi.pfile, &avi.pvideo, &video) != S_OK) {
		return false;
	}
	BITMAPINFOHEADER bmih{};
	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = oip->w;
	bmih.biHeight = oip->h;
	bmih.biPlanes = 1;
	bmih.biBitCount = 24;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = oip->w * oip->h * 3;
	if (AVIStreamSetFormat(avi.pvideo, 0, &bmih, sizeof(bmih)) != S_OK) {
		return false;
	}

	// オーディオストリームの設定
	AVISTREAMINFO audio{};
	audio.fccType = streamtypeAUDIO;
	audio.fccHandler = WAVE_FORMAT_PCM;
	audio.dwSampleSize = oip->audio_ch * 2;
	audio.dwRate = oip->audio_rate * audio.dwSampleSize;
	audio.dwScale = audio.dwSampleSize;
	if (AVIFileCreateStream(avi.pfile, &avi.paudio, &audio) != S_OK) {
		return false;
	}
	WAVEFORMATEX wf{};
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = oip->audio_ch;
	wf.nSamplesPerSec = oip->audio_rate;
	wf.wBitsPerSample = 16;
	wf.nBlockAlign = wf.nChannels * (wf.wBitsPerSample / 8);
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
	if (AVIStreamSetFormat(avi.paudio, 0, &wf, sizeof(wf)) != S_OK) {
		return false;
	}

	oip->func_set_buffer_size(5, 10); // データ取得バッファ数を変更

	for (int frame = 0; frame < oip->n; frame++) {
		oip->func_rest_time_disp(frame, oip->n); // 残り時間を表示
		if (oip->func_is_abort()) break; // 中断の確認

		// ビデオの書き込み
		void* data = oip->func_get_video(frame, BI_RGB);

		// ★ここで加工
		process_frame((uint8_t*)data, oip->w, oip->h);

		if (AVIStreamWrite(avi.pvideo, frame, 1, data, bmih.biSizeImage, AVIIF_KEYFRAME, NULL, NULL) != S_OK) {
			break;
		}

		// オーディオの書き込み
		int audioPos = (int)((double)frame / video.dwRate * video.dwScale * oip->audio_rate);
		int audioNum = (int)((double)(frame + 1) / video.dwRate * video.dwScale * oip->audio_rate) - audioPos;
		int audioReaded = 0;
		data = oip->func_get_audio(audioPos, audioNum, &audioReaded, WAVE_FORMAT_PCM);
		if (audioReaded == 0) continue;
		if (AVIStreamWrite(avi.paudio, audioPos, audioReaded, data, audioReaded * wf.nBlockAlign, 0, NULL, NULL) != S_OK) {
			break;
		}
	}

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
		CheckRadioButton(hDlg, IDC_MODE_WEBSAFE, IDC_MODE_VGA16,
			(g_config.mode == 0) ? IDC_MODE_WEBSAFE : IDC_MODE_VGA16);

		// Bayer
		CheckRadioButton(hDlg, IDC_BAYER_8, IDC_BAYER_4,
			(g_config.bayer == 0) ? IDC_BAYER_8 : IDC_BAYER_4);

		// スライダー
		HWND slider = GetDlgItem(hDlg, IDC_STRENGTH);
		SendMessage(slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200)); // 0.0〜2.0
		SendMessage(slider, TBM_SETPOS, TRUE, (LPARAM)(g_config.strength * 100));

		wchar_t buf[32];
		swprintf_s(buf, L"%.2f", g_config.strength);
		SetDlgItemText(hDlg, IDC_STRENGTH_TEXT, buf);

		return TRUE;
	}

	case WM_HSCROLL:
	{
		HWND slider = GetDlgItem(hDlg, IDC_STRENGTH);
		int pos = (int)SendMessage(slider, TBM_GETPOS, 0, 0);

		double val = pos / 100.0;

		wchar_t buf[32];
		swprintf_s(buf, L"%.2f", val);
		SetDlgItemText(hDlg, IDC_STRENGTH_TEXT, buf);

		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDOK:
		{
			// モード
			g_config.mode =
				(IsDlgButtonChecked(hDlg, IDC_MODE_WEBSAFE) == BST_CHECKED) ? 0 : 1;

			// Bayer
			g_config.bayer =
				(IsDlgButtonChecked(hDlg, IDC_BAYER_8) == BST_CHECKED) ? 0 : 1;

			// 強度
			HWND slider = GetDlgItem(hDlg, IDC_STRENGTH);
			int pos = (int)SendMessage(slider, TBM_GETPOS, 0, 0);
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

