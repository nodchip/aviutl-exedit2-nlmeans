# リリースチェックリスト（ExEdit2 NL-Means）

## 目的

リリース時の抜け漏れを防ぎ、再現可能な手順を維持する。

## 事前確認

1. `git status --short` が空であることを確認する。
2. `master` 最新を取り込む場合は `git pull --ff-only` を使用する。
3. `docs/reports/gpu-coop-benchmark.md` と `docs/reports/gpu-coop-history.csv` の更新有無を確認する。

## 検証

1. `vcvars64.bat` 初期化後に `scripts/run_gtests.cmd` を実行し、全件 PASS を確認する。
2. GPU 環境では `scripts/generate_gpu_coop_report.cmd` を実行する。
3. GPU 環境では `scripts/check_gpu_coop_regression.cmd` と `scripts/check_gpu_coop_async_efficiency.cmd` を実行する。
4. GPU 環境では `scripts/generate_dx11_dx12_quality_report.cmd` と `scripts/check_dx11_dx12_quality_threshold.cmd` を実行する。

## 成果物確認

1. 配布対象に `*.auf2` と必要な設定/ドキュメントが含まれることを確認する。
2. `aviutl2_sdk` が成果物に含まれていないことを確認する。
3. シェーダー配布方針（外部 HLSL 同梱 + 実行時コンパイル）に沿っていることを確認する。
4. 主要レポート（`gpu-coop-benchmark.md`, `dx12-poc-readiness.md`, `dx12-poc-benchmark.md`, `dx11-dx12-quality.md`）が最新であることを確認する。

## タグと記録

1. 変更履歴を確認し、必要なら `HISTORY.md` を更新する。
2. バージョンタグを作成し、リリースノートに build/test コマンド結果を記録する。
3. リリース後に問題が出た場合のロールバック手順を同時に記録する。
