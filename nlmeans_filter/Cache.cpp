#include "stdafx.h"
#include <map>
#include <list>
#include "Cache.h"

using namespace std;

//LRU
template<class KEY_TYPE, class VALUE_TYPE>
Cache<KEY_TYPE, VALUE_TYPE>::Cache() : maxCacheSize(INT_MAX)
{
}

template<class KEY_TYPE, class VALUE_TYPE>
Cache<KEY_TYPE, VALUE_TYPE>::~Cache()
{
}

template<class KEY_TYPE, class VALUE_TYPE>
VALUE_TYPE& Cache<KEY_TYPE, VALUE_TYPE>::get(KEY_TYPE& key)
{
	if (cache.count(key)){
		return cache[key];
	}

	order.push_front(key);
	const VALUE& result = cache[key];

	if (cache.size() > maxCacheSize){
		cache.erase(order.back());
		order.pop_back();
	}

	return result;
}

template<class KEY_TYPE, class VALUE_TYPE>
void Cache<KEY_TYPE, VALUE_TYPE>::add(KEY_TYPE& key, VALUE_TYPE& value)
{
	if (cache.count(key)){
		cache[key] = value;
		return;
	}

	order.push_front(key);
	cache[key] = value;

	if (cache.size() > maxCacheSize){
		cache.erase(order.back());
		order.pop_back();
	}

	return result;
}

template<class KEY_TYPE, class VALUE_TYPE>
bool Cache<KEY_TYPE, VALUE_TYPE>::contains(KEY_TYPE& key) const
{
	return cache.count(key) != 0;
}

template<class KEY_TYPE, class VALUE_TYPE>
void Cache<KEY_TYPE, VALUE_TYPE>::setMaxCacheSize(int maxCacheSize)
{
	this->maxCacheSize = maxCacheSize;
}
