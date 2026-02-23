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
#include <fstream>

// モジュール配置先を基準に対象ファイルのフルパスを組み立てる。
inline bool build_module_relative_path(
	HMODULE module_handle,
	const wchar_t* file_name,
	wchar_t* out_path,
	size_t out_path_length)
{
	if (module_handle == nullptr || file_name == nullptr || out_path == nullptr || out_path_length == 0) {
		return false;
	}

	wchar_t module_path[MAX_PATH] = {};
	const DWORD module_path_length = GetModuleFileNameW(module_handle, module_path, MAX_PATH);
	if (module_path_length == 0 || module_path_length >= MAX_PATH) {
		return false;
	}

	std::wcsncpy(out_path, module_path, out_path_length - 1);
	out_path[out_path_length - 1] = L'\0';
	wchar_t* file_part = std::wcsrchr(out_path, L'\\');
	if (file_part == nullptr) {
		return false;
	}
	*(file_part + 1) = L'\0';
	std::wcsncat(out_path, file_name, out_path_length - std::wcslen(out_path) - 1);
	return true;
}

// プリコンパイル済み CSO を Blob として読み込む。
inline bool load_shader_blob_from_file(const wchar_t* shader_blob_path, ID3DBlob** out_shader_blob)
{
	if (shader_blob_path == nullptr || out_shader_blob == nullptr) {
		return false;
	}
	*out_shader_blob = nullptr;

	std::ifstream input(shader_blob_path, std::ios::binary | std::ios::ate);
	if (!input) {
		return false;
	}

	const std::streamoff byte_size = input.tellg();
	if (byte_size <= 0) {
		return false;
	}
	input.seekg(0, std::ios::beg);

	if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(byte_size), out_shader_blob)) || *out_shader_blob == nullptr) {
		return false;
	}

	input.read(
		static_cast<char*>((*out_shader_blob)->GetBufferPointer()),
		byte_size);
	if (!input) {
		(*out_shader_blob)->Release();
		*out_shader_blob = nullptr;
		return false;
	}

	return true;
}

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
	wchar_t shader_path[MAX_PATH] = {};
	if (build_module_relative_path(module_handle, shader_file_name, shader_path, MAX_PATH)) {
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

// シェーダーを「プリコンパイル済みCSO -> 外部HLSL -> 埋め込みソース」の順で解決する。
inline bool compile_compute_shader_from_cso_or_file_or_embedded(
	HMODULE module_handle,
	const wchar_t* shader_cso_file_name,
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

	wchar_t shader_cso_path[MAX_PATH] = {};
	if (build_module_relative_path(module_handle, shader_cso_file_name, shader_cso_path, MAX_PATH)) {
		if (load_shader_blob_from_file(shader_cso_path, out_shader_blob)) {
			return true;
		}
	}

	return compile_compute_shader_from_file_or_embedded(
		module_handle,
		shader_file_name,
		embedded_source,
		embedded_virtual_name,
		out_shader_blob,
		out_error_blob);
}

#endif
