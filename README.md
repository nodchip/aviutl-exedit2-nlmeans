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
- 複数 GPU 協調に向けた行タイル分割ロジックを追加

## ビルド環境

- Windows
- Visual Studio 2022 Community
- MSBuild 17.x
- DirectX 11.3 以降対応 GPU / ドライバ（`ID3D11Device3` が取得できること）
- GoogleTest（`third_party/googletest` に同梱）

PowerShell からのビルド例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" C:\home\nodchip\nlmeans\nlmeans_filter.sln /p:Configuration=Release /p:Platform=Win32'
cmd.exe /c $cmd
```

PowerShell からのテスト/ベンチ実行例（`vcvars64.bat` 初期化つき）:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && cl /nologo /utf-8 /EHsc /O2 /I .\nlmeans_filter .\nlmeans_filter\tests\NlmKernelBenchmark.cpp /Fe:NlmKernelBenchmark.exe && NlmKernelBenchmark.exe'
cmd.exe /c $cmd
```

GoogleTest テスト実行例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\run_gtests.cmd'
cmd.exe /c $cmd
```

NLM 亜種ベンチレポート生成例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\generate_nlm_variants_report.cmd'
cmd.exe /c $cmd
```

GPU 協調 PoC ベンチレポート生成例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\generate_gpu_coop_report.cmd'
cmd.exe /c $cmd
```

GPU 協調ベンチ履歴を CSV 更新する例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\update_gpu_coop_history.cmd'
cmd.exe /c $cmd
```

GPU 協調ベンチの悪化しきい値を確認する例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\check_gpu_coop_regression.cmd'
cmd.exe /c $cmd
```

GPU 協調の async 効率チェック例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\check_gpu_coop_async_efficiency.cmd'
cmd.exe /c $cmd
```

GPU self-hosted ランナー前提チェック例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\check_gpu_runner_prereq.cmd'
cmd.exe /c $cmd
```

GPU self-hosted ランナー情報レポート生成例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\generate_gpu_runner_health_report.cmd'
cmd.exe /c $cmd
```

GPU 非搭載環境で前提チェックの文法確認のみ行う例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && set ALLOW_NO_GPU=1 && .\scripts\check_gpu_runner_prereq.cmd'
cmd.exe /c $cmd
```

## オプション解説

### 空間範囲

右に行くほど計算範囲が増え、画質が向上します。計算量は `(値*2+1)^2` に比例します。

### 時間範囲

前後フレームを参照してフィルタリングします。計算量は `(値*2+1)` に比例します。

### 分散

右に行くほど平均化（ぼかし）が強くなります。45〜55程度が目安です。

### Fast間引き

`CPU (Fast)` 使用時の探索間引きステップです。値を上げると高速化しやすく、画質はやや低下します。

### Temporal減衰

`CPU (Temporal)` 使用時の時間方向重みの減衰係数です。値を上げるほど過去フレームの寄与が小さくなります。

### GPU間引き

`GPU (DirectX 11)` 使用時の空間探索ステップです。`1` で従来相当、`2` 以上で近似高速化します。

### GPU時間減衰

`GPU (DirectX 11)` 使用時の時間方向重みの減衰係数です。`0` は従来相当です。

### GPU協調数

`GPUアダプタ=Auto` のときに使用する GPU 台数です。2以上で行タイル分割による協調実行を試みます（PoC）。
各 GPU は担当タイル行のみを計算します。

### 計算モード（ExEdit2 現行）

- `CPU (Naive)`
- `CPU (AVX2)`
- `CPU (Fast)`
- `CPU (Temporal)`
- `GPU (DirectX 11)`

## シェーダー配布方式（確定）

- 方式: `外部 HLSL 同梱 + 実行時コンパイル`
- フォールバック: 外部 HLSL 読み込み失敗時は埋め込みシェーダーを実行時コンパイル
- 対象ファイル:
  - `nlmeans_filter/exedit2/nlmeans_exedit2_cs.hlsl`

この方式により、配布後のシェーダー調整（差し替え）と、単体配布時の自己完結性を両立します。

## DirectX 12 PoC

- DX12 PoC の検討メモ: `docs/notes/dx12-poc-2026-02-23.md`
- 現時点ではランタイム検出の最小実装まで導入し、本番経路は DX11.3 を継続しています。

DX12 PoC 可否レポート生成例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\generate_dx12_poc_report.cmd'
cmd.exe /c $cmd
```

DX12 PoC ベンチレポート生成例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\generate_dx12_poc_benchmark.cmd'
cmd.exe /c $cmd
```

DX12 PoC ベンチ履歴 CSV 更新例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\update_dx12_poc_benchmark_history.cmd'
cmd.exe /c $cmd
```

DX12 PoC ベンチ回帰チェック例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\check_dx12_poc_regression.cmd'
cmd.exe /c $cmd
```

DX11 vs DX12 品質比較レポート生成例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\generate_dx11_dx12_quality_report.cmd'
cmd.exe /c $cmd
```

DX11 vs DX12 品質しきい値チェック例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\check_dx11_dx12_quality_threshold.cmd'
cmd.exe /c $cmd
```

DX11 vs DX12 品質履歴 CSV 更新例:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\update_dx11_dx12_quality_history.cmd'
cmd.exe /c $cmd
```

## 注意

- 現在の GPU 実装は最適化途上です。
- 指定モードが実行環境に合わない場合は CPU 処理へ自動フォールバックします。
- 複数GPU協調実行で一部タイルが失敗した場合は、失敗タイルのみ単一GPU実行で再試行します。
- 協調再試行でも回復できない場合は、単一GPU実行へ再試行してから CPU フォールバックします。
- `aviutl2_sdk` はローカル配置前提で `.gitignore` されています。
- GPU self-hosted ランナー運用手順は `docs/operations/gpu-selfhosted-runner.md` を参照してください。
- リリース時の確認手順は `docs/operations/release-checklist.md` を参照してください。
- ExEdit2 実ホストでの E2E 検証手順は `docs/operations/exedit2-e2e-validation.md` を参照してください。

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
