# AviUtl ExEdit2 NL-Means Remake Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** AviUtl NL-Means filter を ExEdit2 向けに再設計し、CPU（Naive/AVX2）と GPU（DirectX 11.3+）を選択可能な構成へ移行する。

**Architecture:** 既存 AviUtl1 + D3D9 実装から、ExEdit2 SDK ベースの新規プラグイン構成へ段階移行する。アルゴリズムは共通コアを CPU/GPU バックエンドで共有し、UI 層から実行デバイスを選択する。最初にリポジトリ整理（UTF-8化・ドキュメント整備・旧SDK除去）を完了し、その後ビルド基盤、CPU/GPU 実装、テストを順次追加する。

**Tech Stack:** C++17/20, Visual Studio 2022 (v143), AviUtl ExEdit2 Plugin SDK, Direct3D 11.3+, HLSL, GoogleTest (or Catch2)

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
