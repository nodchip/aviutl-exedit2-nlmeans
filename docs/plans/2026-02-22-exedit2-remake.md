# AviUtl ExEdit2 NL-Means Remake Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** AviUtl NL-Means filter を ExEdit2 向けに再設計し、CPU（Naive/AVX2）と GPU（DirectX 11.3+）を選択可能な構成へ移行する。

**Architecture:** ExEdit2 SDK ベースのプラグインへ一本化する。AviUtl1 対応は破棄し、ExEdit2 の CPU/GPU バックエンド実装・テスト・UI 統合に集中する。

## 2026-02-22 方針更新

- AviUtl1 対応（`.auf` / `nlmeans_filter.vcxproj` / `filter.h` 系 API 依存）は破棄済み。
- 本計画の AviUtl1 前提タスクは ExEdit2 専用タスクへ読み替える。

**Tech Stack:** C++17/20, Visual Studio 2022 (v143), AviUtl ExEdit2 Plugin SDK, Direct3D 11.3+, HLSL, GoogleTest (or Catch2)

## 進捗サマリ（2026-02-23）

- 完了:
  - Task 1 リポジトリ基盤整理（UTF-8 化、README/HISTORY/LICENSE、SDK ignore）
  - Task 2 VS2022 プロジェクト移行（旧 AviUtl1/旧 vcproj 系は削除）
  - Task 3 CPU 再編（Naive/AVX2 のみ、GoogleTest 整備）
  - Task 7 GitHub 公開準備（CI、公開、運用ドキュメント）
- 進行中:
  - Task 4 GPU バックエンド再設計（DX11 実装 + DX12 PoC 比較/ベンチ拡張 + 履歴CSV/回帰チェック自動化 + compute経路GPU優先化 + fullframe 3x3 compute 実装 + DX11/DX12ベンチ履歴収集 + DX12内訳計測 + HRESULT診断で無効HLSL修正 + preflight一回化 + DLLロードキャッシュ + D3D12Device再利用 + DX11/DX12採用判定ゲート追加 + GPU協調採用判定ゲート追加 + シェーダー解決順をCSO優先へ拡張）
  - Task 5 ExEdit2 UI 統合（モード選択とルーティングは実装済み、実機E2Eの継続検証が必要）
  - Task 6 亜種アルゴリズム実装（Fast/Temporal はCPU中心で実装、GPU 側最適化は継続）
- 未完了の主要項目:
  - DX12 PoC を PoC 段階から本実装（外部HLSL/CSO運用・最適化・エラーハンドリング）へ移行するか、DX11 継続かの最終判断
  - 複数 GPU 協調の性能しきい値と運用判定基準の確定
  - ExEdit2 実ホストでの E2E 検証結果を継続蓄積し、リリース判定を固定化

## DX11/DX12 最終判断ルール（2026-02-23 固定）

- 現在判定（2026-02-23）:
  - `scripts/check_dx11_dx12_adoption_gate.cmd` は FAIL（直近3件の `dx12_compute_ms / dx11_ms` が 1.0 を大幅超過）。
  - 当面の既定実装は DX11 継続とする。
- DX11 固定運用期間:
  - 2026-02-23 から 2026-03-31 までは DX11 を既定実装として固定し、DX12 は検証用途に限定する。
- DX11 継続条件:
  - `scripts/check_dx11_dx12_benchmark_threshold.cmd` が継続 PASS。
  - `scripts/run_gtests.cmd` が継続 PASS。
  - `scripts/generate_dx11_dx12_decision_report.cmd` の `adoption_gate` が FAIL のまま。
- DX12 再判定条件:
  - `scripts/check_dx11_dx12_adoption_gate.cmd` が PASS に到達。
  - 連続3回（別日時）で PASS を再現。
  - ExEdit2 実ホスト E2E でクラッシュ/フォールバック異常がないことを確認。
- DX12 再評価トリガー（2026-03-31 以前でも再評価可能）:
  - `dx12_compute_ms / dx11_ms <= 1.0` を直近3件で満たしたとき。
  - `scripts/check_dx11_dx12_quality_threshold.cmd` が継続 PASS のとき。
  - GPU フォールバック関連テストで不安定挙動が観測されないとき。
- 判定運用:
  - `scripts/run_dx11_dx12_decision_workflow.cmd` を実行し、`docs/reports/dx11-dx12-decision.md` を判断記録として更新する。
  - `scripts/check_dx11_dx12_reevaluation_due.cmd` で次回再評価日の到来状況を確認する。

## GPU協調 判定ルール（2026-02-23 追加）

- 現在判定（2026-02-23）:
  - `scripts/check_gpu_coop_adoption_gate.cmd` は FAIL（single 比で coop ratio が高止まり）。
  - 既定動作は単体GPU（Single GPU Full Frame）継続とする。
- 単体GPU 固定運用期間:
  - 2026-02-23 から 2026-03-31 までは単体GPUを既定動作として固定し、GPU協調は検証用途に限定する。
- GPU協調 採用条件:
  - `scripts/check_gpu_coop_async_efficiency.cmd` が PASS。
  - `scripts/check_gpu_coop_regression.cmd` が PASS。
  - `scripts/check_gpu_coop_adoption_gate.cmd` が PASS（直近3件 ratio と async/single 比が閾値以内）。
- 判定運用:
  - `scripts/run_gpu_coop_decision_workflow.cmd` を実行し、`docs/reports/gpu-coop-decision.md` を更新する。
  - `scripts/check_gpu_coop_reevaluation_due.cmd` で次回再評価日の到来状況を確認する。
  - 採用判定を満たすまでは UI の既定は単体GPU優先を維持する。

---

### Task 1: リポジトリ基盤整理

**Files:**
- Create: `.gitignore`
- Create: `README.md`
- Create: `HISTORY.md`
- Modify: `LICENSE`
- Delete: `readme.txt`
- Delete: `history.txt`
- Delete: `nlmeans_filter/aviutl_plugin_sdk/*`

**Step 1: UTF-8 変換対象の抽出**
Run: `git ls-files` + エンコード検査スクリプト
Expected: Shift_JIS 管理対象を一覧化

**Step 2: Shift_JIS -> UTF-8 変換**
Run: CP932 decode + UTF-8(no BOM) write
Expected: 文字化けなしで内容保持

**Step 3: GitHub 公開向けファイル整理**
Run: `Move-Item LICENSE-2.0.txt LICENSE` など
Expected: `README.md`/`HISTORY.md`/`LICENSE` が存在

**Step 4: SDK 管理方針適用**
Run: `.gitignore` に `nlmeans_filter/aviutl2_sdk/` を追加
Expected: SDK ローカル配置が追跡対象外

**Step 5: Commit**
Run: `git add -A && git commit -m "リポジトリ公開向けの基盤整理"`

### Task 2: VS2022 プロジェクト移行

**Files:**
- Create: `nlmeans_filter.sln`
- Create: `nlmeans_filter/nlmeans_filter.vcxproj`
- Create: `nlmeans_filter/nlmeans_filter.vcxproj.filters`
- Delete: `nlmeans_filter.8.sln`
- Delete: `nlmeans_filter.9.sln`
- Delete: `nlmeans_filter/nlmeans_filter.8.vcproj`
- Delete: `nlmeans_filter/nlmeans_filter.9.vcproj`
- Delete: `nlmeans_filter/nlmf_cuda.vcproj`

**Step 1: 旧プロジェクト依存の棚卸し**
Run: `rg -n "vcproj|\.8\.sln|\.9\.sln"`
Expected: 参照箇所が把握できる

**Step 2: VS2022 用プロジェクト生成**
Run: Visual Studio 2022 で新規 DLL プロジェクト作成
Expected: v143 Toolset でビルド可能

**Step 3: ソース/ヘッダ編成**
Run: 必要ファイルのみプロジェクトへ追加
Expected: 旧実装の不要ファイルを除外

**Step 4: 旧構成削除**
Run: 旧 `.sln/.vcproj` 削除
Expected: VS2022 構成のみ残る

**Step 5: Build**
Run: `msbuild nlmeans_filter.sln /p:Configuration=Release /p:Platform=x64`
Expected: 成功

### Task 3: CPU バックエンド再編（Naive + AVX2）

**Files:**
- Modify: `nlmeans_filter/nlmeans_filter.cpp`
- Modify: `nlmeans_filter/ProcessorCpu.h`
- Modify: `nlmeans_filter/ProcessorCpu.cpp`
- Create: `nlmeans_filter/ProcessorCpuAvx2.h`
- Create: `nlmeans_filter/ProcessorCpuAvx2.cpp`
- Delete: `nlmeans_filter/ProcessorSse2N099.*`
- Delete: `nlmeans_filter/ProcessorSse2Aroo.*`
- Delete: `nlmeans_filter/nlmSse2N099.c`
- Delete: `nlmeans_filter/ProcessorCuda.*`

**Step 1: 失敗テスト作成（演算一致性）**
Run: `ctest` or `gtest` で Naive と AVX2 出力一致テストを追加
Expected: AVX2 未実装で FAIL

**Step 2: AVX2 実装追加**
Run: intrinsics 実装
Expected: テスト PASS

**Step 3: 旧 CPU/CUDA 実装除去**
Run: 不要実装を削除し参照更新
Expected: ビルド通過

**Step 4: ベンチ追加**
Run: 代表サイズで Naive/AVX2 比較
Expected: AVX2 が優位

**Step 5: Commit**
Run: `git add -A && git commit -m "CPUバックエンドをNaive/AVX2へ再編"`

### Task 4: GPU バックエンド再設計（DirectX 11.3+）

**Files:**
- Create: `nlmeans_filter/gpu/d3d11/D3D11Context.*`
- Create: `nlmeans_filter/gpu/d3d11/NlmComputeShader.hlsl`
- Create: `nlmeans_filter/gpu/d3d11/GpuProcessorDx11.*`
- Modify: `nlmeans_filter/Processor.h`
- Modify: `nlmeans_filter/nlmeans_filter.cpp`
- Delete: `nlmeans_filter/ProcessorGpu.*`
- Delete: `nlmeans_filter/PixelShader*`
- Delete: `nlmeans_filter/InputTexture*`

**Step 1: シェーダー配布方式決定**
Run: 方式比較（埋め込みHLSL/外部HLSL/プリコンパイルCSO）
Expected: `外部HLSL同梱 + 実行時コンパイル（外部失敗時は埋め込みフォールバック）` を採択

**Step 2: デバイス列挙と選択 API 実装**
Run: DXGI アダプタ列挙
Expected: 複数 GPU 一覧を UI に渡せる

**Step 3: 単体 GPU 実装**
Run: Compute Shader で NL-Means 実装
Expected: CPU 出力と許容誤差内で一致

**Step 4: 複数 GPU 協調（任意）**
Run: タイル分割 + 非同期実行の PoC
Expected: 正しさ確認

**Step 5: Build + Test**
Run: `msbuild` -> `ctest`
Expected: 成功

### Task 5: ExEdit2 UI 統合

**Files:**
- Modify: `nlmeans_filter/nlmeans_filter.cpp`
- Create: `nlmeans_filter/ui/PluginSettings.*`

**Step 1: 設定項目追加**
Run: UI に `CPU/GPU`, `CPUモード`, `GPUアダプタ` を追加
Expected: 設定画面で選択可能

**Step 2: 設定反映ルーティング**
Run: 実行時にバックエンド切替
Expected: 選択通り動作

**Step 3: フォールバック処理**
Run: GPU 利用不可時 CPU へ自動切替
Expected: 異常終了しない

**Step 4: テスト**
Run: 設定切替の統合テスト
Expected: PASS

**Step 5: Commit**
Run: `git add -A && git commit -m "ExEdit2向け設定UIを実装"`

### Task 6: 亜種アルゴリズム実装

**Files:**
- Create: `nlmeans_filter/algorithms/FastNlm.*`
- Create: `nlmeans_filter/algorithms/TemporalNlm.*`
- Modify: `nlmeans_filter/ProcessorCpu.cpp`
- Modify: `nlmeans_filter/gpu/d3d11/GpuProcessorDx11.*`

**Step 1: 失敗テスト作成（品質/速度）**
Run: PSNR/SSIM と処理時間の閾値テスト
Expected: 未実装で FAIL

**Step 2: Fast NLM 実装**
Run: 探索窓最適化または積分画像系近似を実装
Expected: 速度改善

**Step 3: Temporal 亜種実装**
Run: フレーム間重み付け改善
Expected: ノイズ低減と残像抑制のバランス改善

**Step 4: ベンチ・比較レポート生成**
Run: 固定シード画像で自動比較
Expected: 定量比較結果を Markdown 出力

**Step 5: Commit**
Run: `git add -A && git commit -m "NLM亜種アルゴリズムを追加"`

### Task 7: GitHub 公開準備

**Files:**
- Create: `CONTRIBUTING.md`
- Create: `.github/workflows/ci.yml`
- Modify: `README.md`

**Step 1: リポジトリ名決定**
Run: 候補比較
Expected: `aviutl-exedit2-nlmeans` を第一候補として確定

**Step 2: CI 追加**
Run: Build/Test ワークフロー追加
Expected: push/PR で検証実行

**Step 3: 公開チェック**
Run: `git status --short`, `git log --oneline -5`
Expected: 想定外差分・マージコミットなし

**Step 4: リモート公開**
Run: `git remote add origin ...`, `git push -u origin master`
Expected: GitHub で公開状態

**Step 5: 完了報告**
Run: 実行コマンドと結果を記録
Expected: 再現可能な公開手順が残る
