# GIFベイヤーディザ出力プラグイン

<img src="./docs/x320_vga_c8.gif" width=320 height=320>

減色とベイヤーディザを適用した GIF を出力します。  
画質を犠牲にファイルサイズを削減します。

## 画質、ファイルサイズの比較

解像度: 320x320, 8fps の動画を GIF 出力した結果の比較

### 通常 GIF 出力
<img src="./docs/x320_c256_other.gif" width=320 height=320>

- ファイルサイズ: 4.871 MB
- 「yu7400kiさんの[GIF出力プラグイン](https://yu7400ki.me/aviutl2-animated-image-output/)」による出力

### Webセーフ 216色
<img src="./docs/x320_c216_websafe_bayer.gif" width=320 height=320>

- ファイルサイズ: 2.533 MB

### 選択16色
<img src="./docs/x320_c16_bayer.gif" width=320 height=320>

- ファイルサイズ: 1.721 MB

### VGA 16色

<img src="./docs/x320_vga_c16.gif" width=320 height=320>

- ファイルサイズ: 1.211 MB

### VGA 8色

<img src="./docs/x320_vga_c8.gif" width=320 height=320>

- ファイルサイズ: 0.939 MB

## 設定画面

<img src="./docs/dialog.png" width=352 height=370>
