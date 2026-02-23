# リリースチェックリスト（ExEdit2 NL-Means）

## 目的

リリース時の抜け漏れを防ぎ、再現可能な手順を維持する。

## 事前確認

1. `git status --short` が空であることを確認する。
2. `master` 最新を取り込む場合は `git pull --ff-only` を使用する。
3. `docs/reports/gpu-coop-benchmark.md` と `docs/reports/gpu-coop-history.csv` の更新有無を確認する。

## 検証

1. `vcvars64.bat` 初期化後に `scripts/run_gtests.cmd` を実行し、全件 PASS を確認する。
2. DX11/DX12 判定時は `scripts/run_dx11_dx12_decision_workflow.cmd` を実行し、判定レポートを更新する。
3. DX11/DX12 判定時は `scripts/check_dx11_dx12_reevaluation_due.cmd` で再評価日の到来状況を確認する。
4. GPU 環境では `scripts/generate_gpu_coop_report.cmd` を実行する。
5. GPU 環境では `scripts/check_gpu_coop_regression.cmd` と `scripts/check_gpu_coop_async_efficiency.cmd` を実行する。
6. GPU 環境では `scripts/generate_dx11_dx12_quality_report.cmd` と `scripts/check_dx11_dx12_quality_threshold.cmd` を実行する。
7. GPU 環境では `scripts/update_dx11_dx12_quality_history.cmd` を実行し、履歴 CSV を更新する。
8. GPU 環境では `scripts/update_dx12_poc_benchmark_history.cmd` を実行し、DX12 PoC ベンチ履歴 CSV を更新する。
9. GPU 環境では `scripts/check_dx12_poc_regression.cmd` を実行し、DX12 PoC compute path の回帰を確認する。

## 成果物確認

1. 配布対象に `*.auf2` と必要な設定/ドキュメントが含まれることを確認する。
2. `aviutl2_sdk` が成果物に含まれていないことを確認する。
3. シェーダー配布方針（外部 HLSL 同梱 + 実行時コンパイル）に沿っていることを確認する。
4. 主要レポート（`gpu-coop-benchmark.md`, `dx12-poc-readiness.md`, `dx12-poc-benchmark.md`, `dx11-dx12-quality.md`, `dx11-dx12-decision.md`）が最新であることを確認する。

## DX11/DX12 判断基準

1. 品質: `check_dx11_dx12_quality_threshold.cmd` が PASS（最大差分・平均差分が既定閾値以内）であること。
2. 速度: `dx11-dx12-benchmark-history.csv` で直近 3 計測の `dx12_compute_ms / dx11_ms <= 1.0` を継続して満たすこと。
3. 安定性: `run_gtests.cmd` が連続 3 回 PASS し、GPU フォールバック関連テストに不安定要素がないこと。
4. 運用: `dx11-dx12-quality-history.csv` に判断対象期間の履歴が残り、回帰傾向がないこと。
5. 総合判定: `scripts/check_dx11_dx12_adoption_gate.cmd` が PASS すること。
6. 期限管理: `scripts/check_dx11_dx12_reevaluation_due.cmd` の結果を判定記録へ残すこと。

## タグと記録

1. 変更履歴を確認し、必要なら `HISTORY.md` を更新する。
2. バージョンタグを作成し、リリースノートに build/test コマンド結果を記録する。
3. リリース後に問題が出た場合のロールバック手順を同時に記録する。
