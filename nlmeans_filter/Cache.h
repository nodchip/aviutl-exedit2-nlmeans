#ifndef CACHE_H
#define CACHE_H

#include <map>
#include <list>

template<class KEY_TYPE, class VALUE_TYPE>
class Cache
{
public:
	Cache() : maxCacheSize(INT_MAX) {}
	virtual ~Cache(){}
	VALUE_TYPE& get(const KEY_TYPE& key)
	{
		if (cache.count(key)){
			return cache[key];
		}

		order.push_front(key);
		VALUE_TYPE& result = cache[key];

		if ((int)cache.size() > maxCacheSize){
			cache.erase(order.back());
			order.pop_back();
		}

		return result;

	}
	void add(const KEY_TYPE& key, const VALUE_TYPE& value)
	{
		if (cache.count(key)){
			cache[key] = value;
			return;
		}

		order.push_front(key);
		cache[key] = value;

		if ((int)cache.size() > maxCacheSize){
			cache.erase(order.back());
			order.pop_back();
		}
	}
	bool contains(const KEY_TYPE& key) const
	{
		return cache.count(key) != 0;

	}
	void clear()
	{
		cache.clear();
		order.clear();
	}
	void setMaxCacheSize(int maxCacheSize)
	{
		this->maxCacheSize = maxCacheSize;
	}
private:
	int maxCacheSize;
	std::map<KEY_TYPE, VALUE_TYPE> cache;
	std::list<KEY_TYPE> order;
};

#endif
