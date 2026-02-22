# AviUtl ExEdit2 NL-Means Filter by nodchip

- Original site: http://kishibe.dyndns.tv/
- License: Apache License 2.0

## プロジェクト状況（2026-02-22）

このリポジトリは `AviUtl NL-Means filter by nodchip` の ExEdit2 向けリメイク作業中の状態です。
現時点では以下を実施済みです。

- ExEdit2 専用プロジェクトへ移行（AviUtl1 対応は破棄）
- 旧 SDK (`aviutl_plugin_sdk`) の削除
- DirectX 11 Compute Shader バックエンドの導入
- GPU の空間 + 時間方向 NL-Means（暫定実装）

## ビルド環境

- Windows
- Visual Studio 2022 Community
- MSBuild 17.x
- DirectX 11 対応 GPU / ドライバ
- GoogleTest（`third_party/googletest` に同梱）

PowerShell からのビルド例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" C:\home\nodchip\nlmeans\nlmeans_filter.sln /p:Configuration=Release /p:Platform=Win32'
cmd.exe /c $cmd
```

PowerShell からのテスト/ベンチ実行例（`vcvars64.bat` 初期化つき）:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\NlmKernelTests.cpp /Fe:NlmKernelTests.exe && NlmKernelTests.exe && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\NlmFrameReferenceTests.cpp /Fe:NlmFrameReferenceTests.exe && NlmFrameReferenceTests.exe && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\NlmFrameOutputTests.cpp /Fe:NlmFrameOutputTests.exe && NlmFrameOutputTests.exe && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\NlmFrameOutputMultiCaseTests.cpp /Fe:NlmFrameOutputMultiCaseTests.exe && NlmFrameOutputMultiCaseTests.exe && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\BackendSelectionTests.cpp /Fe:BackendSelectionTests.exe && BackendSelectionTests.exe && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\GpuAdapterSelectionTests.cpp /Fe:GpuAdapterSelectionTests.exe && GpuAdapterSelectionTests.exe && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\ExecutionPolicyTests.cpp /Fe:ExecutionPolicyTests.exe && ExecutionPolicyTests.exe && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\GpuFallbackPolicyTests.cpp /Fe:GpuFallbackPolicyTests.exe && GpuFallbackPolicyTests.exe && cl /nologo /utf-8 /EHsc /O2 /I .\nlmeans_filter .\nlmeans_filter\tests\NlmKernelBenchmark.cpp /Fe:NlmKernelBenchmark.exe && NlmKernelBenchmark.exe'
cmd.exe /c $cmd
```

GoogleTest テスト実行例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && cl /nologo /utf-8 /EHsc /std:c++17 /I .\third_party\googletest\googletest\include /I .\third_party\googletest\googletest /I .\nlmeans_filter .\third_party\googletest\googletest\src\gtest-all.cc .\third_party\googletest\googletest\src\gtest_main.cc .\nlmeans_filter\tests\ModeIdsGoogleTest.cpp .\nlmeans_filter\tests\ExecutionPolicyTests.cpp /Fe:GoogleTests.exe && .\GoogleTests.exe'
cmd.exe /c $cmd
```

## オプション解説

### 空間範囲

右に行くほど計算範囲が増え、画質が向上します。計算量は `(値*2+1)^2` に比例します。

### 時間範囲

前後フレームを参照してフィルタリングします。計算量は `(値*2+1)` に比例します。

### 分散

右に行くほど平均化（ぼかし）が強くなります。45〜55程度が目安です。

### 計算モード（ExEdit2 現行）

- `CPU (Naive)`
- `CPU (AVX2)`
- `GPU (DirectX 11)`

## シェーダー配布方式（確定）

- 方式: `外部 HLSL 同梱 + 実行時コンパイル`
- フォールバック: 外部 HLSL 読み込み失敗時は埋め込みシェーダーを実行時コンパイル
- 対象ファイル:
  - `nlmeans_filter/exedit2/nlmeans_exedit2_cs.hlsl`

この方式により、配布後のシェーダー調整（差し替え）と、単体配布時の自己完結性を両立します。

## 注意

- 現在の GPU 実装は最適化途上です。
- 指定モードが実行環境に合わない場合は CPU 処理へ自動フォールバックします。
- `aviutl2_sdk` はローカル配置前提で `.gitignore` されています。

## 既知の不具合

- AVX2 最適化は継続中です。
- GPU 実装の品質/速度チューニングは継続中です。

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
