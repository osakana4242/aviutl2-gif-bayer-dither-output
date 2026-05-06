//----------------------------------------------------------------------------------
//	GIFベイヤーディザ出力プラグイン for AviUtl ExEdit2
//----------------------------------------------------------------------------------

#define NOMINMAX

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
#include "common.h"

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
	L"GIFベイヤーディザ出力", // プラグインの名前
	L"GIF File (*.gif)\0*.gif\0", // 出力ファイルのフィルタ
	L"GIFベイヤーディザ出力 v1.0.0 By 三川おさかな", // プラグインの情報
	func_output, // 出力時に呼ばれる関数へのポインタ
	func_config, // 出力設定のダイアログを要求された時に呼ばれる関数へのポインタ (nullptrなら呼ばれません)
	func_get_config_text, // 出力設定のテキスト情報を取得する時に呼ばれる関数へのポインタ (nullptrなら呼ばれません)
};



//---------------------------------------------------------------------
// 設定構造体
//---------------------------------------------------------------------
struct CONFIG {
	BayerDitherConfig bayerDither;
};

static CONFIG g_config;



//---------------------------------------------------------------------

ColorMapObject* create_palette() {
	const unsigned char (*palette)[3] = nullptr;
	size_t size = get_palette(g_config.bayerDither, palette);

	auto* map = GifMakeMapObject(size, NULL);

	for (int i = 0; i < size; i++) {
		map->Colors[i].Red =   palette[i][0];
		map->Colors[i].Green = palette[i][1];
		map->Colors[i].Blue =  palette[i][2];
	}
	return map;
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

inline int calc_stride(int w) {
	return (w * 3 + 3) & ~3;
}

#include <unordered_map>
#include <cstdint>

std::vector<HistColor> collect_histogram(OUTPUT_INFO* oip, int w, int h, int stride) {

	const int pixel_step = 1;
	const int frame_step = 1;

	std::unordered_map<uint32_t, uint32_t> map;
	map.reserve(100000);

	int shift = g_config.bayerDither.color_shift;

	for (int frame = 0; frame < oip->n; frame += frame_step) {

		if (oip->func_is_abort()) break;

		uint8_t* src = (uint8_t*)oip->func_get_video(frame, BI_RGB);
		if (!src) continue;

		for (int y = 0; y < oip->h; y += pixel_step) {

			uint8_t* row = src + y * stride;

			for (int x = 0; x < oip->w; x += pixel_step) {

				uint8_t* p = row + x * 3;

				uint32_t key;
				uint8_t r = p[2];
				uint8_t g = p[1];
				uint8_t b = p[0];
				r >>= shift;
				g >>= shift;
				b >>= shift;

				key =
					(r << 16) |
					(g << 8) |
					b;

				map[key]++;
			}
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


bool func_output(OUTPUT_INFO* oip) {

	int error = 0;
	std::string path = wide_to_utf8(oip->savefile);
	GifFileType* gif = EGifOpenFileName(path.c_str(), false, &error);
	if (!gif) return false;

	int stride = calc_stride(oip->w);

	if (g_config.bayerDither.mode == ColorMode::Custom) {
		auto samples = collect_histogram(oip, oip->w, oip->h, stride);
		auto palette = median_cut_histogram(samples, g_config.bayerDither.color_count);
		palette_custom_size = g_config.bayerDither.color_count;
		for (size_t i = 0; i < palette.size(); i++) {
			palette_custom[i][0] = palette[i].r;
			palette_custom[i][1] = palette[i].g;
			palette_custom[i][2] = palette[i].b;
		}
	}

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

		convert_to_indexed(g_config.bayerDither, src, indexed.data(), oip->w, oip->h, stride);

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


void process_frame(uint8_t* data, int w, int h, int stride) {
	for (int y = 0; y < h; y++) {
		uint8_t* row = data + y * stride;

		for (int x = 0; x < w; x++) {
			uint8_t* px = row + x * 3;

			uint8_t b = px[0];
			uint8_t g = px[1];
			uint8_t r = px[2];

			quantize(g_config.bayerDither, r, g, b, x, y, r, g, b);

			px[0] = b;
			px[1] = g;
			px[2] = r;
		}
	}
}

bool func_output_avi(OUTPUT_INFO* oip) {
	AVI_HANDLE avi;
	if (AVIFileOpen(&avi.pfile, oip->savefile, OF_WRITE | OF_CREATE, NULL) != S_OK) {
		return false;
	}

	// ===== stride 計算 =====
	int stride = calc_stride(oip->w);

	// ===== ビデオストリーム設定 =====
	AVISTREAMINFO video{};
	video.fccType = streamtypeVIDEO;
	video.fccHandler = 0; // ← BI_RGBじゃなくてこっち
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
	bmih.biSizeImage = stride * oip->h; // ← 修正ポイント

	if (AVIStreamSetFormat(avi.pvideo, 0, &bmih, sizeof(bmih)) != S_OK) {
		return false;
	}

	// ===== オーディオ（そのまま） =====
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

	oip->func_set_buffer_size(5, 10);

	for (int frame = 0; frame < oip->n; frame++) {
		oip->func_rest_time_disp(frame, oip->n);
		if (oip->func_is_abort()) break;


		uint8_t* data = (uint8_t*)oip->func_get_video(frame, BI_RGB);

		// ===== エフェクト適用 =====
		process_frame(data, oip->w, oip->h, stride);

		AVIStreamWrite(
				avi.pvideo,
				frame,
				1,
				data,
				stride * oip->h,
				AVIIF_KEYFRAME,
				NULL,
				NULL
		);

		// ===== オーディオ =====
		int audioPos = (int)((double)frame / video.dwRate * video.dwScale * oip->audio_rate);
		int audioNum = (int)((double)(frame + 1) / video.dwRate * video.dwScale * oip->audio_rate) - audioPos;

		int audioReaded = 0;
		void* aud = oip->func_get_audio(audioPos, audioNum, &audioReaded, WAVE_FORMAT_PCM);

		if (audioReaded == 0) continue;

		if (AVIStreamWrite(
			avi.paudio,
			audioPos,
			audioReaded,
			aud,
			audioReaded * wf.nBlockAlign,
			0,
			NULL,
			NULL) != S_OK) {
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
		HWND hMode = GetDlgItem(hDlg, IDC_MODE);
		for (int i = 0; i < (int)ColorMode::Count; i++) {
			SendMessageW(hMode, CB_ADDSTRING, 0,
				(LPARAM)g_mode_display_names[i]);
		}
		SendMessageW(hMode, CB_SETCURSEL, (int)g_config.bayerDither.mode, 0);

		// Bayer
		HWND hBayer = GetDlgItem(hDlg, IDC_BAYER);
		for (int i = 0; i < (int)BayerMode::Count; i++) {
			SendMessageW(hBayer, CB_ADDSTRING, 0,
				(LPARAM)g_bayer_display_names[i]);
		}
		SendMessageW(hBayer, CB_SETCURSEL, (int)g_config.bayerDither.bayer, 0);

		// スライダー
		HWND slider = GetDlgItem(hDlg, IDC_STRENGTH);
		SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200)); // 0.0〜2.0
		SendMessageW(slider, TBM_SETPOS, TRUE, (LPARAM)(g_config.bayerDither.strength * 100));
		SendMessageW(slider, TBM_SETTICFREQ, 10, 0); // 0.1刻みの目盛り

		wchar_t buf[32];
		swprintf_s(buf, L"%.2f", g_config.bayerDither.strength);
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
			g_config.bayerDither.mode = (ColorMode)SendMessageW(
					GetDlgItem(hDlg, IDC_MODE), CB_GETCURSEL, 0, 0);

			// Bayer
			g_config.bayerDither.bayer = (BayerMode)SendMessageW(
					GetDlgItem(hDlg, IDC_BAYER), CB_GETCURSEL, 0, 0);

			// 強度
			HWND slider = GetDlgItem(hDlg, IDC_STRENGTH);
			int pos = (int)SendMessageW(slider, TBM_GETPOS, 0, 0);
			g_config.bayerDither.strength = pos / 100.0;

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

	const wchar_t* mode = g_mode_display_names[(int)g_config.bayerDither.mode];

	const wchar_t* bayer =g_bayer_display_names[(int)g_config.bayerDither.bayer];

	swprintf_s(buf, L"%s / Bayer %s / 強度 %.2f",
						 mode, bayer, g_config.bayerDither.strength);

	return buf;
}
