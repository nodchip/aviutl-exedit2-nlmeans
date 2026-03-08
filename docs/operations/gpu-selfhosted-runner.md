# GPU Self-Hosted Runner 運用手順

## 目的

`gpu-selfhosted-ci.yml` を安定運用するために、ランナー側の前提条件と確認手順を固定化する。

## 前提

- Windows ランナーに GitHub Actions Runner が導入済み
- ラベル: `self-hosted`, `windows`, `gpu`
- Visual Studio 2022 (MSBuild / cl) を導入済み
- ExEdit2 SDK を `nlmeans_filter/aviutl2_sdk/` に配置済み
- GPU ドライバが有効で `Win32_VideoController` から列挙できる

## 初回セットアップ確認

PowerShell で以下を実行する。

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\check_gpu_runner_prereq.cmd'
cmd.exe /c $cmd
```

期待結果:

- `[check_gpu_runner_prereq] ok`
- GPU アダプタ一覧が表示される

続けて、ランナー情報レポートを生成する。

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && .\scripts\generate_gpu_runner_health_report.cmd'
cmd.exe /c $cmd
```

期待結果:

- `docs/reports/gpu-runner-health.md` が生成される

補足:

- 開発端末など GPU 非搭載環境で文法確認だけ行う場合は `ALLOW_NO_GPU=1` を付けて実行できる。
- CI では `ALLOW_NO_GPU` を設定せず、GPU 0 件を失敗として扱う。

## ワークフロー失敗時の確認順序

1. `Check GPU Runner Prerequisites` のログで欠落要件を確認する。
2. `Build Release x64` の失敗時は `MSBuild.exe` と `filter2.h` の存在を再確認する。
3. `Run GoogleTest Suite` の失敗時はローカルで `scripts/run_gtests.cmd` を再実行する。
4. `Check GPU Coop Regression Threshold` の失敗時は `docs/reports/gpu-coop-history.csv` の直近2行を確認する。
5. `Check GPU Coop Async Efficiency` の失敗時は `docs/reports/gpu-coop-benchmark.md` の `sequential/async` 行を確認する。
6. ランナー依存の問題が疑われる場合は `docs/reports/gpu-runner-health.md` を確認する。

## 保守メモ

- `scripts/check_gpu_runner_prereq.cmd` のチェック項目変更時は、この文書と `CONTRIBUTING.md` も同時更新する。
- GPU を交換した場合は、ワークフロー実行前に前提チェックを再実行する。
