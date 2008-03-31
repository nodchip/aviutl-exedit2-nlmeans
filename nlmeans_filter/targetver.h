// Copyright 2008 nod_chip
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

// 以下のマクロは、最低限必要なプラットフォームを定義します。最低限必要なプラットフォームとは、
// アプリケーションを実行するために必要な機能を備えた最も古いバージョンの Windows や Internet Explorer など
// をいいます。これらのマクロは、指定したバージョンと、それ以前のバージョンのプラットフォーム上で利用できるすべての機能を有効にすることによって 
// 動作します。

// 下で指定された定義の前に対象プラットフォームを指定しなければならない場合、以下の定義を変更してください。
// 異なるプラットフォームに対応する値に関する最新情報については、MSDN を参照してください。
#ifndef WINVER                          // 最低限必要なプラットフォームが Windows Vista であることを指定します。
#define WINVER 0x0600           // これを Windows の他のバージョン向けに適切な値に変更してください。
#endif

#ifndef _WIN32_WINNT            // 最低限必要なプラットフォームが Windows Vista であることを指定します。
#define _WIN32_WINNT 0x0600     // これを Windows の他のバージョン向けに適切な値に変更してください。
#endif

#ifndef _WIN32_WINDOWS          // 最低限必要なプラットフォームが Windows 98 であることを指定します。
#define _WIN32_WINDOWS 0x0410 // これを Windows Me またはそれ以降のバージョン向けに適切な値に変更してください。
#endif

#ifndef _WIN32_IE                       // 最低限必要なプラットフォームが Internet Explorer 7.0 であることを指定します。
#define _WIN32_IE 0x0700        // これを IE の他のバージョン向けに適切な値に変更してください。
#endif
