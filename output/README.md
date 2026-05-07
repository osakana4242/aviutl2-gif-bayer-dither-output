# GIFベイヤーディザ出力プラグイン

<img src="./docs/x320_vga_c8.gif" width=320 height=320>

減色とベイヤーディザを適用した GIF を出力します。  
画質を犠牲にファイルサイズを削減します。

## 画質・ファイルサイズの比較

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

## インストール手順

1. `GifBayerDither-v1.0.0.zip` などを解凍
2. `GifBayerDither.auo2` を実行中の
   `AviUtl2.exe` のウィンドウにドラッグ＆ドロップ

## 比較に使用した動画素材

以下の Pixabay の動画素材を使用しています。

- [ねこ](https://pixabay.com/ja/videos/%e7%8c%ab-%e3%82%aa%e3%83%ac%e3%83%b3%e3%82%b8%e8%89%b2%e3%81%ae%e7%8c%ab-%e9%a3%bc%e3%81%84%e7%8c%ab-179212/)
- [とり](https://pixabay.com/ja/videos/%e3%82%aa%e3%82%a6%e3%83%a0-%e9%b3%a5-%e5%8b%95%e7%89%a9-%e7%be%bd-23223/)
- [かめ](https://pixabay.com/ja/videos/%e3%82%ab%e3%83%a1-%e6%b5%b7%e6%b4%8b-%e9%87%8e%e7%94%9f%e5%8b%95%e7%89%a9-%e6%b0%b4%e4%b8%ad-244754/)

## ライセンス

0BSD License

詳細は [LICENSE.txt](./LICENSE.txt) を参照してください。
