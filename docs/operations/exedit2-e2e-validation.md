# ExEdit2 E2E 検証手順

## 目的

ExEdit2 実ホスト上で CPU/GPU の動作とフォールバック経路を確認する。

## 事前準備

1. AviUtl ExEdit2 の動作環境を用意する。
2. `nlmeans_filter\exedit2` のビルド成果物（`.auf2`）を配置する。
3. `nlmeans_filter/aviutl2_sdk/filter2.h` がローカルに存在することを確認する。

## テスト素材

- 1280x720、30fps、5秒程度のノイズ入り動画
- 同一素材を複製し、比較確認しやすい状態にする

## 検証項目

1. `CPU (Naive)` が正常に処理される。
2. `CPU (AVX2)` が `CPU (Naive)` と破綻なく一致する。
3. `GPU (DirectX 11)` が正常に処理される。
4. `GPUアダプタ=Auto`, `GPU協調数=2` で協調実行される。
5. GPU 利用不可環境で CPU フォールバックが働く。

## 実施手順

1. 同一フレームに対して `CPU (Naive)` と `CPU (AVX2)` の出力を確認する。
2. `GPU (DirectX 11)` に切り替えて同一フレームの破綻有無を確認する。
3. `GPU協調数=2` に設定し、処理時間と出力の破綻有無を確認する。
4. 一時的に GPU モードを無効化できる環境でフォールバック動作を確認する。
5. 問題があれば再現手順（設定値、入力素材、フレーム位置）を記録する。

## 記録テンプレート

- 実施日:
- 実施者:
- 使用ビルド:
- 使用GPU:
- 検証結果:
  - CPU (Naive):
  - CPU (AVX2):
  - GPU (DirectX 11):
  - GPU協調:
  - フォールバック:
- 備考:

## 記録の蓄積方法

1. 検証結果を `docs/reports/exedit2-e2e-history.csv` に追記する。
2. 追記は `scripts/append_exedit2_e2e_result.cmd` を使い、判定値は `PASS|FAIL|SKIP` のいずれかを使う。

実行例:

```cmd
scripts\append_exedit2_e2e_result.cmd ^
  nodchip ^
  nlmeans_filter_exedit2.auf2 ^
  "NVIDIA GeForce RTX 3060" ^
  PASS PASS PASS PASS PASS ^
  "AviUtl ExEdit2 実機検証"
```

3. リリース前は `scripts/check_exedit2_e2e_gate.cmd` を実行し、最新記録が有効か確認する。
