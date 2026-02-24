# NL-Means Filter (ExEdit2)

AviUtl ExEdit2 用のノイズ除去フィルタです。  
NL-Means による空間方向のノイズ低減に加えて、時間方向（過去フレーム参照）のノイズ低減も選べます。

- フィルタ名: `NL-Means Filter (ExEdit2)`（表示名） / `NL-Means`（短縮名）
- Original: `AviUtl NL-Means filter by nodchip`（http://kishibe.dyndns.tv/）
- License: Apache License 2.0（`LICENSE`）

## できること

- ノイズ除去（ディテール保持寄り）
- CPU/GPU の切り替え（`計算モード`）
- 時間方向の参照（`時間範囲`）によるちらつき低減（`CPU (Temporal)` / `GPU (DirectX 11)`）

## NL-Means とは

NL-Means は、探索範囲内の「似ている画素（パッチ）」ほど重く、異なるほど軽く重み付けして平均化することでノイズを低減する手法です。  
エッジや模様を比較的保ちやすい一方、設定次第では処理が重くなります。

## 動作環境

- Windows 10 以降（64bit）
- AviUtl ExEdit2
- ExEdit2 専用です（AviUtl1 / 拡張編集では動作しません）
- AVX2 対応 CPU（ExEdit2 の要件に準拠）
- GPU モード利用時: DirectX 11.3 以降に対応した GPU / ドライバ（ExEdit2 の要件に準拠）

## 導入（インストール）

1. ExEdit2 を終了します。
2. 配布された `.zip` を任意の場所に展開します。
3. 展開してできた `nlmeans_filter_exedit2` フォルダを、`C:\ProgramData\aviutl2\Plugin\` 配下にコピーします。
4. ExEdit2 を起動します。

ヒント:

- `C:\ProgramData` は隠しフォルダです。エクスプローラーのアドレスバーに `C:\ProgramData\aviutl2\Plugin\`（または `%ProgramData%\aviutl2\Plugin`）を貼り付けると開けます。

配置例:

```text
C:\ProgramData\aviutl2\Plugin\nlmeans_filter_exedit2\
  nlmeans_filter_exedit2.auf2
  nlmeans_exedit2_cs.hlsl
  nlmeans_exedit2_cs.cso    (任意)
  README.md
  LICENSE
```

## 使い方（設定画面の開き方）

1. ノイズ除去したいオブジェクト（映像/画像など）を選択します。
2. 「オブジェクト設定」内のフィルタ追加から `NL-Means Filter (ExEdit2)` を追加します。
3. 追加したフィルタの項目（トラックバー/リスト）で設定を調整します。

ポイント:

- 迷ったら `計算モード=GPU (DirectX 11)`、`空間範囲=3`、`時間範囲=0`、`分散=50` から始めてください。
- ノイズ除去は素材の上流（加工の早い段階）に入れると調整しやすいです。

## 設定項目

### 計算モード（`計算モード`）

- `GPU (DirectX 11)`: GPU（Compute Shader）で処理します。高速です。GPU が利用できない場合は CPU へ自動フォールバックします。
- `CPU (AVX2)`: CPU（AVX2）で処理します。`CPU (Naive)` と同等画質を目標にしています。
- `CPU (Naive)`: 参照実装です。最も遅いですが比較用に使えます。
- `CPU (Fast)`: 近似（間引き）で高速化した CPU 版です。画質より速度重視のときに使います。
- `CPU (Temporal)`: 過去フレームも参照する CPU 版です。ノイズが強い素材やちらつき低減に有効な場合があります。

### 画質/性能（共通）

- `空間範囲`: 探索範囲の半径です。大きいほどノイズは減りやすい一方、処理は重くなります。
- `分散`: 平均化の強さです。大きいほど強くノイズをならします（目安 45-55）。
- `Fast間引き`: `CPU (Fast)` の探索ステップです。上げると高速化しやすい一方、画質は低下しやすくなります。

### 時間方向（Temporal）

- `時間範囲`: 参照する過去フレーム数です。`0` で時間方向の参照なしになります。
- `Temporal減衰`: `CPU (Temporal)` の過去フレームの重み減衰です。大きいほど過去フレームの影響が弱くなります。

注意:

- 動きが大きい素材やカット切り替え付近では、時間方向参照により残像が出ることがあります。`時間範囲` を下げるか、減衰を上げてください。

### GPU 関連

- `GPUアダプタ`: 使用する GPU を選びます。`Auto` は自動選択です。複数 GPU 環境では明示指定もできます。
- `GPU間引き`: `GPU (DirectX 11)` の近似（探索ステップ）です。上げると高速化しやすい一方、画質は低下しやすくなります。
- `GPU時間減衰`: `GPU (DirectX 11)` の過去フレームの重み減衰です。
- `GPU協調数`: 実験的項目です。通常は `1` を推奨します（複数 GPU 協調はサポート対象外です）。

## おすすめ設定（目安）

- 標準（まずはここから）
  - `計算モード=GPU (DirectX 11)`
  - `空間範囲=3`
  - `時間範囲=0`
  - `分散=50`
- 強め（ノイズが強い素材）
  - `空間範囲=4-6`
  - `分散=55-70`
- ちらつき低減（Temporal）
  - `時間範囲=1-2`
  - `GPU時間減衰=0.5-1.5` または `Temporal減衰=0.5-1.5`

## トラブルシューティング

- フィルタが一覧に出ない
  - `C:\ProgramData\aviutl2\Plugin\nlmeans_filter_exedit2\nlmeans_filter_exedit2.auf2` が存在するか確認し、ExEdit2 を再起動してください。
- `LoadLibrary()` 失敗 / 「有効な Win32 アプリケーションではありません」
  - ExEdit2（64bit）用プラグインです。AviUtl1 で読み込んでいないか、`.auf2` を配置しているか確認してください。
- `GPU (DirectX 11)` で動かない/落ちる
  - `CPU (AVX2)` に切り替えて回避してください。GPU ドライバ更新、`GPUアダプタ` の変更で改善する場合があります。
- 初回だけ処理開始が遅い
  - シェーダーの読み込み/（必要なら）コンパイルが走る場合があります。`nlmeans_exedit2_cs.hlsl` を同梱しておくと確実です。

## アンインストール

ExEdit2 を終了し、`C:\ProgramData\aviutl2\Plugin\nlmeans_filter_exedit2\` フォルダを削除してください。

## ライセンスとクレジット

- License: Apache License 2.0（`LICENSE`）
- Original: `AviUtl NL-Means filter by nodchip`（http://kishibe.dyndns.tv/）
