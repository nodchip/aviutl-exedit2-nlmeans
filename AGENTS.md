# AGENTS.md（nlmeans プロジェクト固有）

## 目的
- AviUtl ExEdit2 向けリメイク作業で発生した手順ミスを、再発防止ルールとして固定化する。

## 実行・検証ルール

### Visual Studio / MSBuild
- ビルド実行前に `MSBuild.exe` の所在を `vswhere` で特定し、`Test-Path` で存在確認する。
- `msbuild` コマンドを直接呼ばず、検出した `MSBuild.exe` の絶対パスで実行する。
- Visual Studio 2022 のビルド前に `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` を実行し、コンパイラ関連の環境変数を初期化する。
- PowerShell からは `cmd /c "\"<vcvars64.batのパス>\" && msbuild ..."` 形式で同一プロセス内に環境を引き継いで実行する。
- 完了報告で build 成功を主張する場合は、実行コマンドと終了コードを記録する。

### PowerShell でのプロジェクトファイル生成
- `.vcxproj` / `.sln` のように `$(...)` を含むテキストを生成するときは、シングルクォートのヒアストリング `@' ... '@` を使う。
- ダブルクォートのヒアストリング `@" ... "@` は PowerShell 変数展開で `$(Configuration)` 等を壊すため使わない。
- 生成直後に `Select-String -Pattern '\$\((Configuration|Platform|VCTargetsPath|SolutionDir)\)'` でプレースホルダ残存を確認する。

### 文字コード変換（Shift_JIS -> UTF-8）
- 変換対象は `git ls-files` の追跡ファイルに限定し、未追跡ファイルを一括変換しない。
- 判定は `UTF-8(DecoderExceptionFallback) で失敗` かつ `CP932 で成功` の条件で実施する。
- 変換後は同じ判定を再実行し、`Shift_JIS 判定 0 件` を完了条件とする。

### SDK 取り扱い
- `nlmeans_filter/aviutl2_sdk/` はローカル参照専用とし、`.gitignore` で除外する。
- SDK 更新時はリポジトリに取り込まず、バージョン情報は `README.md` に記録する。
