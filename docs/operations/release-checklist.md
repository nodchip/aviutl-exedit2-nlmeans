# リリースチェックリスト（ExEdit2 NL-Means）

## 目的

リリース時の抜け漏れを防ぎ、再現可能な手順を維持する。

## 事前確認

1. `git status --short` が空であることを確認する。
2. `master` 最新を取り込む場合は `git pull --ff-only` を使用する。
3. `docs/reports/gpu-coop-benchmark.md` と `docs/reports/gpu-coop-history.csv` の更新有無を確認する。
4. リリース方針を確認する（Task 6 は凍結、`fallback=SKIP` は許容仕様）。

## 検証

1. `vcvars64.bat` 初期化後に `scripts/run_gtests.cmd` を実行し、全件 PASS を確認する。
2. GPU 環境では `scripts/run_gpu_coop_decision_workflow.cmd` を実行し、協調判定レポートを更新する。
3. GPU 環境では `scripts/check_gpu_coop_regression.cmd` と `scripts/check_gpu_coop_async_efficiency.cmd` の結果を確認する。
4. GPU 亜種ベンチとして `scripts/generate_gpu_variants_report.cmd` を実行し、間引き/時間減衰の傾向を確認する。
5. 実ホスト検証結果として `scripts/check_exedit2_e2e_gate.cmd` を実行し、E2E 記録の最新性と判定条件（`fallback=PASS|SKIP`）を確認する。
6. 実ホスト検証結果の要約として `scripts/generate_exedit2_e2e_report.cmd` を実行し、`docs/reports/exedit2-e2e-status.md` を更新する。
7. 全体の残課題を `scripts/generate_remake_remaining_tasks_report.cmd` で更新し、判断時点の未完了事項を記録する。

## 成果物確認

1. 配布対象に `*.auf2` と必要な設定/ドキュメントが含まれることを確認する。
2. `aviutl2_sdk` が成果物に含まれていないことを確認する。
3. シェーダー配布方針（プリコンパイルCSO優先、外部 HLSL + 実行時コンパイル、埋め込みフォールバック）に沿っていることを確認する。
4. 主要レポート（`gpu-coop-benchmark.md`, `gpu-coop-decision.md`, `exedit2-e2e-status.md`, `remake-remaining-tasks.md`, `gpu-variants-benchmark.md`）が最新であることを確認する。
5. ExEdit2 実機検証レポート（`exedit2-e2e-status.md`）が最新であることを確認する。
6. リメイク残タスクレポート（`remake-remaining-tasks.md`）が最新であることを確認する。
7. GPU 亜種ベンチレポート（`gpu-variants-benchmark.md`）が最新であることを確認する。

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
