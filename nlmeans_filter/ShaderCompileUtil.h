// Copyright 2026 nodchip
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

#ifndef SHADER_COMPILE_UTIL_H
#define SHADER_COMPILE_UTIL_H

#include <windows.h>
#include <d3dcompiler.h>
#include <cwchar>
#include <cstring>

// シェーダーを「外部ファイル優先、失敗時に埋め込みソース」でコンパイルする。
inline bool compile_compute_shader_from_file_or_embedded(
	HMODULE module_handle,
	const wchar_t* shader_file_name,
	const char* embedded_source,
	const char* embedded_virtual_name,
	ID3DBlob** out_shader_blob,
	ID3DBlob** out_error_blob)
{
	if (out_shader_blob == nullptr) {
		return false;
	}
	*out_shader_blob = nullptr;
	if (out_error_blob != nullptr) {
		*out_error_blob = nullptr;
	}

	bool compiled = false;
	wchar_t module_path[MAX_PATH] = {};
	if (module_handle != nullptr && GetModuleFileNameW(module_handle, module_path, MAX_PATH) > 0) {
		wchar_t shader_path[MAX_PATH] = {};
		std::wcsncpy(shader_path, module_path, MAX_PATH - 1);
		wchar_t* file_part = std::wcsrchr(shader_path, L'\\');
		if (file_part != nullptr) {
			*(file_part + 1) = L'\0';
			if (shader_file_name != nullptr) {
				std::wcsncat(shader_path, shader_file_name, MAX_PATH - std::wcslen(shader_path) - 1);
				if (SUCCEEDED(D3DCompileFromFile(
					shader_path,
					nullptr,
					D3D_COMPILE_STANDARD_FILE_INCLUDE,
					"main",
					"cs_5_0",
					0,
					0,
					out_shader_blob,
					out_error_blob))) {
					compiled = true;
				}
			}
		}
	}

	if (!compiled) {
		if (embedded_source == nullptr) {
			return false;
		}
		if (FAILED(D3DCompile(
			embedded_source,
			std::strlen(embedded_source),
			embedded_virtual_name,
			nullptr,
			nullptr,
			"main",
			"cs_5_0",
			0,
			0,
			out_shader_blob,
			out_error_blob))) {
			return false;
		}
	}

	return *out_shader_blob != nullptr;
}

#endif
