//----------------------------------------------------------------------------------
//	サンプルAVI(vfw経由)出力プラグイン for AviUtl ExEdit2
//----------------------------------------------------------------------------------

#include <algorithm>
#include <windows.h>
#include <vfw.h>
#pragma comment(lib, "vfw32.lib")

#include "output2.h"

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

inline unsigned char quantize_websafe(int v, int x, int y) {
    double d = (bayer8x8[y & 7][x & 7] / 64.0) - 0.5;
    double val = v + d * 51.0;

    val = std::clamp(val, 0.0, 255.0);
    int level = (int)(val / 51.0 + 0.5);
    level = std::clamp(level, 0, 5);

    return (unsigned char)(level * 51);
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

            r = quantize_websafe(r, x, y);
            g = quantize_websafe(g, x, y);
            b = quantize_websafe(b, x, y);

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
EXTERN_C __declspec(dllexport) OUTPUT_PLUGIN_TABLE* GetOutputPluginTable(void)
{
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
bool func_config(HWND hwnd, HINSTANCE dll_hinst) {
	MessageBox(hwnd, L"サンプルダイアログ", L"出力設定", MB_OK);

	// DLLを開放されても設定が残るように保存しておいてください

	return true;
}

//---------------------------------------------------------------------
//	出力設定のテキスト情報 (設定ボタンの隣に表示される)
//---------------------------------------------------------------------
LPCWSTR func_get_config_text() {
	return L"サンプル設定情報";
}
