// ShaderCompileUtil の CSO 優先解決を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../ShaderCompileUtil.h"

#include <fstream>
#include <string>
#include <vector>

namespace {

std::wstring get_module_directory(HMODULE module)
{
	wchar_t module_path[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameW(module, module_path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) {
		return L"";
	}

	std::wstring directory(module_path);
	const std::wstring::size_type last_sep = directory.find_last_of(L"\\/");
	if (last_sep == std::wstring::npos) {
		return L"";
	}
	directory.resize(last_sep + 1);
	return directory;
}

bool write_bytes_to_file(const std::wstring& path, const std::vector<unsigned char>& bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output) {
		return false;
	}
	output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return output.good();
}

} // namespace

TEST(ShaderCompileUtilTests, UsesPrecompiledCsoWhenPresent)
{
	const HMODULE module = GetModuleHandleW(nullptr);
	ASSERT_NE(module, nullptr);

	const std::wstring file_name = L"shader_compile_util_test_blob.cso";
	const std::wstring file_path = get_module_directory(module) + file_name;
	const std::vector<unsigned char> expected_bytes = { 0x01, 0x23, 0x45, 0x67, 0x89 };

	ASSERT_TRUE(write_bytes_to_file(file_path, expected_bytes));

	ID3DBlob* shader_blob = nullptr;
	ID3DBlob* error_blob = nullptr;
	const bool ok = compile_compute_shader_from_cso_or_file_or_embedded(
		module,
		file_name.c_str(),
		L"missing_shader_file.hlsl",
		nullptr,
		nullptr,
		&shader_blob,
		&error_blob);

	DeleteFileW(file_path.c_str());

	ASSERT_TRUE(ok);
	ASSERT_NE(shader_blob, nullptr);
	ASSERT_EQ(shader_blob->GetBufferSize(), expected_bytes.size());

	const unsigned char* actual_bytes = static_cast<const unsigned char*>(shader_blob->GetBufferPointer());
	ASSERT_NE(actual_bytes, nullptr);
	for (size_t i = 0; i < expected_bytes.size(); ++i) {
		EXPECT_EQ(actual_bytes[i], expected_bytes[i]);
	}

	if (error_blob != nullptr) {
		error_blob->Release();
	}
	shader_blob->Release();
}

TEST(ShaderCompileUtilTests, ReturnsFalseWhenNoSourceIsAvailable)
{
	const HMODULE module = GetModuleHandleW(nullptr);
	ASSERT_NE(module, nullptr);

	ID3DBlob* shader_blob = nullptr;
	ID3DBlob* error_blob = nullptr;
	const bool ok = compile_compute_shader_from_cso_or_file_or_embedded(
		module,
		L"missing_shader_blob.cso",
		L"missing_shader_file.hlsl",
		nullptr,
		nullptr,
		&shader_blob,
		&error_blob);

	EXPECT_FALSE(ok);
	EXPECT_EQ(shader_blob, nullptr);
	if (error_blob != nullptr) {
		error_blob->Release();
	}
}
