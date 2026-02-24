// ExEdit2 の選択リスト項目を安全に構築するヘルパー。
#ifndef EXEDIT2_SELECT_ITEM_BUILDER_H
#define EXEDIT2_SELECT_ITEM_BUILDER_H

#include <cstddef>
#include <string>
#include <vector>

// 名前配列から選択項目配列を再構築する。
// name ポインタは names 側の要素を参照するため、呼び出し後に names を変更しないこと。
template<typename ItemT>
inline void rebuild_select_items_from_names(
	const std::vector<std::wstring>& names,
	std::vector<ItemT>* outItems)
{
	if (outItems == nullptr) {
		return;
	}

	outItems->clear();
	outItems->reserve(names.size() + 1);
	for (size_t i = 0; i < names.size(); ++i) {
		outItems->push_back({ names[i].c_str(), static_cast<int>(i) });
	}
	outItems->push_back({ nullptr, 0 });
}

#endif
