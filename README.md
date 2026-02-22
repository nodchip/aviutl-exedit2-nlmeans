# AviUtl NL-Means Filter by nodchip

- Original site: http://kishibe.dyndns.tv/
- License: Apache License 2.0

## はじめに

知人から2000年代初頭に新しく考案されたノイズ除去フィルタの話を聞き、AviUtl フィルタプラグインとして実装したものです。
GPU 並列処理と OpenMP を使ったマルチコア CPU 並列処理も実装されています。

## 動作環境（旧実装）

- CPU のみ: AviUtl の動作環境に準拠
- GPU 使用時:
  - DirectX 9 ランタイム
  - NVIDIA GeForce 6 ファミリ以降、または ATI RADEON X1000 ファミリ以降

## インストール（旧実装）

`nlmeans_filter.auf` と `vcomp90.dll` を AviUtl と同じフォルダに配置します。

## 使用法

プラグインを有効にすると動作します。

## オプション解説

### 空間範囲

右に行くほど計算範囲が増え、画質が向上します。計算量は `(値*2+1)^2` に比例します。

### 時間範囲

前後フレームを参照してフィルタリングします。計算量は `(値*2+1)` に比例します。

### 分散

右に行くほど平均化（ぼかし）が強くなります。45〜55程度が目安です。

### 計算モード（旧実装）

- `0`: nodchip リファレンス実装（低速・高精度）
- `1`: N099 氏 SSE2 実装（高速・時間方向未対応）
- `2`: Aroo 氏 SSE2 実装（さらに高速・時間方向対応）
- `3`: nodchip GPU 実装（高速・Shader Model 3.0 以降が必要）

## 注意

- CPU と GPU は分散パラメータのスケーリングが異なります。
- 指定モードが実行環境に合わない場合は CPU 処理へ自動フォールバックします。
- GPU 利用時は発熱に注意してください。

## 既知の不具合

- 処理時間が遅いのは仕様です。

## NL-Means アルゴリズム概要

NL-Means は、設定範囲内の類似領域を探索し、重み付き平均でノイズ除去を行います。
エッジ近傍ではエッジ情報を比較的保持しやすく、アニメ画像などで効果が高い手法です。

## GPU 並列実行の概要

各画素を独立に計算できる性質を利用し、テクスチャ入力とレンダリング処理で並列化しています。

## 開発・テスト環境（当時）

- PentiumM 1.83GHz + NVIDIA GeForce 6800 Go
- Intel Core 2 Quad Q6700 + NVIDIA GeForce 9600 GT
- Intel Core 2 Quad Q6600 + ATI RADEON HD 2400

## 参考文献

- Antoni Buades, Bartomeu Coll, Jean-Michel Morel,
  "A Non-Local Algorithm for Image Denoising," CVPR 2005.