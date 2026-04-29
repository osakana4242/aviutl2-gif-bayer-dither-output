//----------------------------------------------------------------------------------
//  GIF 出力プラグイン for AviUtl ExEdit2
//  Ordered Dithering (Bayer 8x8)
//----------------------------------------------------------------------------------

#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>
#include <windows.h>

#include "output2.h"

bool func_output(OUTPUT_INFO *oip);
bool func_config(HWND hwnd, HINSTANCE dll_hinst);
LPCWSTR func_get_config_text();

//---------------------------------------------------------------------
// プラグイン構造体
//---------------------------------------------------------------------

OUTPUT_PLUGIN_TABLE output_plugin_table = {
    OUTPUT_PLUGIN_TABLE::FLAG_VIDEO,

    L"GIF File Saver2 (Ordered Dither)",
    L"GIF File (*.gif)\0*.gif\0",
    L"GIF Saver with Ordered Dithering",

    func_output,
    func_config,
    func_get_config_text,
};

EXTERN_C __declspec(dllexport) OUTPUT_PLUGIN_TABLE *GetOutputPluginTable(void) {
  return &output_plugin_table;
}

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD) { return true; }

EXTERN_C __declspec(dllexport) void UninitializePlugin() {}

////////////////////////////////////////////////////////////////
// Ordered Dither
////////////////////////////////////////////////////////////////

static const int Bayer8x8[8][8] = {
    {0, 48, 12, 60, 3, 51, 15, 63}, {32, 16, 44, 28, 35, 19, 47, 31},
    {8, 56, 4, 52, 11, 59, 7, 55},  {40, 24, 36, 20, 43, 27, 39, 23},
    {2, 50, 14, 62, 1, 49, 13, 61}, {34, 18, 46, 30, 33, 17, 45, 29},
    {10, 58, 6, 54, 9, 57, 5, 53},  {42, 26, 38, 22, 41, 25, 37, 21}};

inline uint8_t quantize_channel(uint8_t value, int x, int y, int levels) {
  int threshold = Bayer8x8[y & 7][x & 7];

  float v = value / 255.0f;

  v += (threshold / 64.0f - 0.5f) / levels;

  int q = (int)(v * (levels - 1) + 0.5f);

  if (q < 0)
    q = 0;
  if (q >= levels)
    q = levels - 1;

  return (uint8_t)q;
}

inline uint8_t quantize_rgb(uint8_t r, uint8_t g, uint8_t b, int x, int y) {
  int rq = quantize_channel(r, x, y, 8);
  int gq = quantize_channel(g, x, y, 8);
  int bq = quantize_channel(b, x, y, 4);

  return (rq << 5) | (gq << 2) | bq;
}

////////////////////////////////////////////////////////////////
// Palette
////////////////////////////////////////////////////////////////

static void build_palette(uint8_t palette[256][3]) {
  for (int r = 0; r < 8; ++r)
    for (int g = 0; g < 8; ++g)
      for (int b = 0; b < 4; ++b) {
        int index = (r << 5) | (g << 2) | b;

        palette[index][0] = r * 255 / 7;
        palette[index][1] = g * 255 / 7;
        palette[index][2] = b * 255 / 3;
      }
}

////////////////////////////////////////////////////////////////

#include <unordered_map>

class GifLZW {
public:
  static void encode(std::ofstream &file, const uint8_t *data, int size,
                     int minCodeSize) {

    const int CLEAR = 1 << minCodeSize;
    const int END = CLEAR + 1;

    int codeSize = minCodeSize + 1;
    int nextCode = END + 1;

    uint32_t bitBuffer = 0;
    int bitCount = 0;

    std::vector<uint8_t> block;
    block.reserve(256);

    auto flushBlock = [&]() {
      if (block.empty()) {
        return;
      }

      file.put((uint8_t)block.size());
      file.write((const char *)block.data(), block.size());

      block.clear();
    };

    auto writeBits = [&](int code) {
      bitBuffer |= (code << bitCount);
      bitCount += codeSize;

      while (bitCount >= 8) {
        block.push_back(bitBuffer & 0xFF);

        if (block.size() == 255) {
          flushBlock();
        }

        bitBuffer >>= 8;
        bitCount -= 8;
      }
    };

    file.put(minCodeSize);

    writeBits(CLEAR);

    int prefix = data[0];

    std::unordered_map<uint32_t, int> dict;

    for (int i = 1; i < size; ++i) {
      int c = data[i];

      uint32_t key = (prefix << 8) | c;

      auto it = dict.find(key);

      if (it != dict.end()) {
        prefix = it->second;
      } else {
        writeBits(prefix);

        if (nextCode < 4096) {
          dict[key] = nextCode++;
        } else {
          writeBits(CLEAR);

          dict.clear();

          codeSize = minCodeSize + 1;
          nextCode = END + 1;
        }

        prefix = c;

        if (nextCode == (1 << codeSize) && codeSize < 12) {
          codeSize++;
        }
      }
    }

    writeBits(prefix);
    writeBits(END);

    if (bitCount > 0) {
      block.push_back(bitBuffer);
    }

    flushBlock();

    file.put(0);
  }
};

////////////////////////////////////////////////////////////////
// 超最小 GIF Writer
////////////////////////////////////////////////////////////////

class GifWriter {
public:
  std::ofstream file;

  bool open(const wchar_t *path, int w, int h) {
    file.open(path, std::ios::binary);
    if (!file)
      return false;

    write_header(w, h);
    return true;
  }

  void close() { file.put(0x3B); }

  void write_frame(const uint8_t *index, int w, int h, int delay_cs) {
    write_graphics_control(delay_cs);
    write_image_descriptor(w, h);
    write_image_data(index, w, h);
  }

private:
  void write_header(int w, int h) {
    file.write("GIF89a", 6);

    write16(w);
    write16(h);

    file.put(0xF7);
    file.put(0);
    file.put(0);

    uint8_t palette[256][3];
    build_palette(palette);

    for (int i = 0; i < 256; ++i) {
      file.put(palette[i][0]);
      file.put(palette[i][1]);
      file.put(palette[i][2]);
    }
  }

  void write_graphics_control(int delay) {
    file.put(0x21);
    file.put(0xF9);
    file.put(4);
    file.put(0);
    write16(delay);
    file.put(0);
    file.put(0);
  }

  void write_image_descriptor(int w, int h) {
    file.put(0x2C);

    write16(0);
    write16(0);

    write16(w);
    write16(h);

    file.put(0);
  }

  void write_image_data(const uint8_t *index, int w, int h) {
    int size = w * h;

    GifLZW::encode(file, index, size, 8);
  }

  void write16(int v) {
    file.put(v & 255);
    file.put(v >> 8);
  }
};

////////////////////////////////////////////////////////////////
// frame convert
////////////////////////////////////////////////////////////////

static void dither_frame(const uint8_t *src, uint8_t *dst, int w, int h) {
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      int i = (y * w + x) * 3;

      uint8_t b = src[i + 0];
      uint8_t g = src[i + 1];
      uint8_t r = src[i + 2];

      dst[y * w + x] = quantize_rgb(r, g, b, x, y);
    }
}

////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////

bool func_output(OUTPUT_INFO *oip) {
  int w = oip->w;
  int h = oip->h;

  GifWriter gif;

  if (!gif.open(oip->savefile, w, h))
    return false;

  std::vector<uint8_t> indexBuffer(w * h);

  int delay = (int)(100.0 * oip->scale / oip->rate);

  for (int frame = 0; frame < oip->n; ++frame) {
    oip->func_rest_time_disp(frame, oip->n);

    if (oip->func_is_abort())
      break;

    void *data = oip->func_get_video(frame, BI_RGB);

    dither_frame((uint8_t *)data, indexBuffer.data(), w, h);

    gif.write_frame(indexBuffer.data(), w, h, delay);
  }

  gif.close();

  return true;
}

////////////////////////////////////////////////////////////////
// config
////////////////////////////////////////////////////////////////

bool func_config(HWND hwnd, HINSTANCE) {
  MessageBox(hwnd, L"Ordered Dither GIF", L"設定", MB_OK);

  return true;
}

LPCWSTR func_get_config_text() { return L"256色 / Ordered Dither"; }
