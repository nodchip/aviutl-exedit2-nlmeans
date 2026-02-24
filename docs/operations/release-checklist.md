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
4. GPU 環境では `scripts/run_gpu_coop_decision_workflow.cmd` を実行し、協調判定レポートを更新する。
5. GPU 環境では `scripts/check_gpu_coop_regression.cmd` と `scripts/check_gpu_coop_async_efficiency.cmd` の結果を確認する。
6. GPU 環境では `scripts/generate_dx11_dx12_quality_report.cmd` と `scripts/check_dx11_dx12_quality_threshold.cmd` を実行する。
7. GPU 環境では `scripts/update_dx11_dx12_quality_history.cmd` を実行し、履歴 CSV を更新する。
8. GPU 環境では `scripts/update_dx12_poc_benchmark_history.cmd` を実行し、DX12 PoC ベンチ履歴 CSV を更新する。
9. GPU 環境では `scripts/check_dx12_poc_regression.cmd` を実行し、DX12 PoC compute path の回帰を確認する。
10. 実ホスト検証結果として `scripts/check_exedit2_e2e_gate.cmd` を実行し、E2E 記録の最新性と必須項目 PASS を確認する。
11. 実ホスト検証結果の要約として `scripts/generate_exedit2_e2e_report.cmd` を実行し、`docs/reports/exedit2-e2e-status.md` を更新する。

## 成果物確認

1. 配布対象に `*.auf2` と必要な設定/ドキュメントが含まれることを確認する。
2. `aviutl2_sdk` が成果物に含まれていないことを確認する。
3. シェーダー配布方針（プリコンパイルCSO優先、外部 HLSL + 実行時コンパイル、埋め込みフォールバック）に沿っていることを確認する。
4. 主要レポート（`gpu-coop-benchmark.md`, `gpu-coop-decision.md`, `dx12-poc-readiness.md`, `dx12-poc-benchmark.md`, `dx11-dx12-quality.md`, `dx11-dx12-decision.md`）が最新であることを確認する。
5. ExEdit2 実機検証レポート（`exedit2-e2e-status.md`）が最新であることを確認する。

## DX11/DX12 判断基準

1. 品質: `check_dx11_dx12_quality_threshold.cmd` が PASS（最大差分・平均差分が既定閾値以内）であること。
2. 速度: `dx11-dx12-benchmark-history.csv` で直近 3 計測の `dx12_compute_ms / dx11_ms <= 1.0` を継続して満たすこと。
3. 安定性: `run_gtests.cmd` が連続 3 回 PASS し、GPU フォールバック関連テストに不安定要素がないこと。
4. 運用: `dx11-dx12-quality-history.csv` に判断対象期間の履歴が残り、回帰傾向がないこと。
5. 総合判定: `scripts/check_dx11_dx12_adoption_gate.cmd` が PASS すること。
6. 期限管理: `scripts/check_dx11_dx12_reevaluation_due.cmd` の結果を判定記録へ残すこと。
7. 実ホスト確認: `scripts/check_exedit2_e2e_gate.cmd` が PASS し、E2E 記録が有効期間内であること。

## GPU協調 判断基準

1. 効率: `check_gpu_coop_async_efficiency.cmd` が PASS（async dispatch が sequential dispatch 比で既定閾値以内）であること。
2. 回帰: `check_gpu_coop_regression.cmd` が PASS（直近比較で ratio/coop_ms が既定悪化率以内）であること。
3. 採用判定: `check_gpu_coop_adoption_gate.cmd` が PASS（直近3件の ratio と async/single 比が閾値以内）であること。
4. 総合判定: `run_gpu_coop_decision_workflow.cmd` 実行後の `gpu-coop-decision.md` を判定記録として残すこと。
5. 期限管理: `check_gpu_coop_reevaluation_due.cmd` の結果を判定記録へ残すこと。
6. 実ホスト確認: `scripts/check_exedit2_e2e_gate.cmd` が PASS し、`gpu_coop` が `PASS` もしくは `SKIP` として記録されていること。

## タグと記録

1. 変更履歴を確認し、必要なら `HISTORY.md` を更新する。
2. バージョンタグを作成し、リリースノートに build/test コマンド結果を記録する。
3. リリース後に問題が出た場合のロールバック手順を同時に記録する。
