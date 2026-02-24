// 選択項目配列構築ヘルパーを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/SelectItemBuilder.h"

namespace {
struct TestSelectItem {
	const wchar_t* name;
	int value;
};
}

TEST(SelectItemBuilderTests, BuildsIndicesAndNullTerminator)
{
	const std::vector<std::wstring> names = { L"Auto", L"NVIDIA GeForce RTX 4070" };
	std::vector<TestSelectItem> items;
	rebuild_select_items_from_names(names, &items);

	ASSERT_EQ(items.size(), 3u);
	EXPECT_STREQ(items[0].name, L"Auto");
	EXPECT_EQ(items[0].value, 0);
	EXPECT_STREQ(items[1].name, L"NVIDIA GeForce RTX 4070");
	EXPECT_EQ(items[1].value, 1);
	EXPECT_EQ(items[2].name, nullptr);
	EXPECT_EQ(items[2].value, 0);
}

TEST(SelectItemBuilderTests, KeepsPointersToCurrentNameStorage)
{
	std::vector<std::wstring> names;
	names.push_back(L"Auto");
	for (int i = 0; i < 16; ++i) {
		names.push_back(std::wstring(L"Adapter ") + std::to_wstring(i));
	}

	std::vector<TestSelectItem> items;
	rebuild_select_items_from_names(names, &items);

	ASSERT_EQ(items.size(), names.size() + 1);
	for (size_t i = 0; i < names.size(); ++i) {
		EXPECT_EQ(items[i].name, names[i].c_str());
		EXPECT_EQ(items[i].value, static_cast<int>(i));
	}
	EXPECT_EQ(items.back().name, nullptr);
	EXPECT_EQ(items.back().value, 0);
}
