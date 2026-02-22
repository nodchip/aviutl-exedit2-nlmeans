# Contributing

## 目的

このリポジトリは `AviUtl ExEdit2` 向け `NL-Means` フィルタの開発を行います。  
機能追加・不具合修正・ドキュメント更新は歓迎します。

## 事前確認

1. 変更前に `git status --short` が空であることを確認してください。
2. ビルド・テストは Visual Studio 2022 の環境変数初期化後に実行してください。
3. 1コミット1目的を維持してください。

## ビルド

PowerShell から実行する場合:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" C:\home\nodchip\nlmeans\nlmeans_filter.sln /p:Configuration=Release /p:Platform=Win32 /m:1'
cmd.exe /c $cmd
```

## テスト

PowerShell から実行する場合:

```powershell
$cmd='"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && cl /nologo /utf-8 /EHsc /I .\nlmeans_filter .\nlmeans_filter\tests\NlmKernelTests.cpp /Fe:NlmKernelTests.exe && NlmKernelTests.exe'
cmd.exe /c $cmd
```

主要テスト:

- `NlmKernelTests.cpp`
- `NlmFrameReferenceTests.cpp`
- `NlmFrameOutputTests.cpp`
- `NlmFrameOutputMultiCaseTests.cpp`
- `BackendSelectionTests.cpp`
- `GpuAdapterSelectionTests.cpp`
- `ExecutionPolicyTests.cpp`
- `GpuFallbackPolicyTests.cpp`

## コーディング規約

- 文字コードは UTF-8（BOM なし）を使用します。
- コメントは日本語で記述します。
- コミットメッセージは日本語で記述します。

## Pull Request

1. 変更内容を説明し、目的を1つに絞ってください。
2. 実行した build / test コマンドと結果を記載してください。
3. 既知の制約や未検証範囲があれば明記してください。
