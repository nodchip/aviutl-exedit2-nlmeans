# Contributing

## 目的

このリポジトリは `AviUtl ExEdit2` 向け `NL-Means` フィルタの開発を行います。  
機能追加・不具合修正・ドキュメント更新は歓迎します。

## 事前確認

1. 変更前に `git status --short` が空であることを確認してください。
2. ビルド・テストは Visual Studio 2022 の環境変数初期化後に実行してください。
3. 1コミット1目的を維持してください。

## ビルド

PowerShell から実行する場合:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" C:\home\nodchip\nlmeans\nlmeans_filter.sln /p:Configuration=Release /p:Platform=Win32 /m:1'
cmd.exe /c $cmd
```

## テスト

PowerShell から実行する場合:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\run_gtests.cmd'
cmd.exe /c $cmd
```

主要テスト:

- `NlmKernelTests.cpp`（GoogleTest）
- `NlmFrameReferenceTests.cpp`（GoogleTest）
- `NlmFrameOutputTests.cpp`（GoogleTest）
- `NlmFrameOutputMultiCaseTests.cpp`（GoogleTest）
- `ModeIdsGoogleTest.cpp`（GoogleTest）
- `ExecutionPolicyTests.cpp`（GoogleTest）
- `GpuAdapterSelectionTests.cpp`（GoogleTest）
- `BackendSelectionTests.cpp`（GoogleTest）
- `ProcessingRoutePolicyTests.cpp`（GoogleTest）
- `GpuFallbackPolicyTests.cpp`（GoogleTest）
- `GpuFallbackExecutionTests.cpp`（GoogleTest）
- `GpuQualityComparisonTests.cpp`（GoogleTest, SDK配置時）
- `GpuRunnerDispatchTests.cpp`（GoogleTest）
- `UiSelectionRouteTests.cpp`（GoogleTest）
- `UiToDispatcherIntegrationTests.cpp`（GoogleTest）
- `VideoProcessingDispatcherTests.cpp`（GoogleTest）

## コーディング規約

- 文字コードは UTF-8（BOM なし）を使用します。
- コメントは日本語で記述します。
- コミットメッセージは日本語で記述します。

## テストフレームワーク

- GoogleTest を `third_party/googletest` に同梱しています。
- GoogleTest テストは `scripts/run_gtests.cmd` で実行してください。
- `GpuQualityComparisonTests.cpp` は `aviutl2_sdk/filter2.h` がない環境では自動で `SKIP` になります。

## ベンチレポート

- `scripts/generate_nlm_variants_report.cmd` で `docs/reports/nlm-variants-benchmark.md` を生成できます。
- GitHub Actions CI では同レポートを artifact (`nlm-variants-benchmark`) として保存します。
- `scripts/generate_gpu_coop_report.cmd` で `docs/reports/gpu-coop-benchmark.md` を生成できます。
- GitHub Actions CI では同レポートを artifact (`gpu-coop-benchmark`) として保存します。

## Pull Request

1. 変更内容を説明し、目的を1つに絞ってください。
2. 実行した build / test コマンドと結果を記載してください。
3. 既知の制約や未検証範囲があれば明記してください。

## GPU 実機 CI

- `.github/workflows/gpu-selfhosted-ci.yml` は `self-hosted, windows, gpu` ラベルのランナー向けです。
- GPU 品質比較と協調ベンチを含む結果を artifact (`gpu-selfhosted-reports`) に保存します。
