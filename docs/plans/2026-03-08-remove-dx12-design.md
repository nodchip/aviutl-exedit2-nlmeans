# DX12 Removal Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** ExEdit2 版 NL-Means から DX12 PoC 実装・検証導線を撤去し、DX11 前提の構成へ整理する。

**Architecture:** 実行バックエンドは CPU 系と `GPU (DirectX 11)` のみを残す。DX12 専用ヘッダ、テスト、ベンチ、判定スクリプト、関連ドキュメント参照を削除し、バックエンド判定は DX11 を明示的に扱う実装へ寄せる。

**Tech Stack:** C++17, Visual Studio 2022, GoogleTest, CMD/PowerShell scripts

---

### Task 1: 境界値テスト追加

**Files:**
- Modify: `nlmeans_filter/tests/BackendSelectionTests.cpp`
- Modify: `nlmeans_filter/tests/ProcessingRoutePolicyTests.cpp`

**Step 1: Write the failing test**
- 未定義モード値が GPU として扱われないことを検証する。

**Step 2: Run test to verify it fails**
- `scripts/run_gtests.cmd`

**Step 3: Write minimal implementation**
- GPU 判定を `kModeGpuDx11` のみへ限定する。

**Step 4: Run test to verify it passes**
- `scripts/run_gtests.cmd`

### Task 2: DX12 実装とテストの撤去

**Files:**
- Delete: `nlmeans_filter/exedit2/Dx12PocBackend.h`
- Delete: `nlmeans_filter/exedit2/Dx12PocProcessor.h`
- Delete: `nlmeans_filter/tests/Dx12PocBackendTests.cpp`
- Delete: `nlmeans_filter/tests/Dx12PocProcessorTests.cpp`
- Delete: `nlmeans_filter/tests/Dx12PocProbe.cpp`
- Delete: `nlmeans_filter/tests/Dx12PocBenchmark.cpp`
- Delete: `nlmeans_filter/tests/Dx11Dx12Benchmark.cpp`
- Delete: `nlmeans_filter/tests/Dx11Dx12QualityReport.cpp`
- Modify: `scripts/run_gtests.cmd`

### Task 3: スクリプトとドキュメント整理

**Files:**
- Delete: `scripts/generate_dx12_poc_report.cmd`
- Delete: `scripts/generate_dx12_poc_benchmark.cmd`
- Delete: `scripts/update_dx12_poc_benchmark_history.cmd`
- Delete: `scripts/check_dx12_poc_regression.cmd`
- Delete: `scripts/generate_dx11_dx12_benchmark.cmd`
- Delete: `scripts/generate_dx11_dx12_quality_report.cmd`
- Delete: `scripts/update_dx11_dx12_benchmark_history.cmd`
- Delete: `scripts/update_dx11_dx12_quality_history.cmd`
- Delete: `scripts/check_dx11_dx12_benchmark_threshold.cmd`
- Delete: `scripts/check_dx11_dx12_quality_threshold.cmd`
- Delete: `scripts/check_dx11_dx12_adoption_gate.cmd`
- Delete: `scripts/check_dx11_dx12_reevaluation_due.cmd`
- Delete: `scripts/generate_dx11_dx12_decision_report.cmd`
- Delete: `scripts/run_dx11_dx12_decision_workflow.cmd`
- Modify: `README.md`
- Modify: `CONTRIBUTING.md`
- Modify: `docs/operations/release-checklist.md`
- Modify: `docs/operations/gpu-selfhosted-runner.md`
- Modify: `docs/plans/2026-02-22-exedit2-remake.md`

### Task 4: 検証

**Files:**
- Modify: なし

**Step 1: Build**
- `MSBuild.exe nlmeans_filter.sln /p:Configuration=Release /p:Platform=x64`

**Step 2: Test**
- `scripts/run_gtests.cmd`

**Step 3: Residual scan**
- `rg -n "Dx12|DX12|dx12" -S`
