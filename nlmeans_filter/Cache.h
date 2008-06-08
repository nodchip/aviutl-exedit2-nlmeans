#ifndef CACHE_H
#define CACHE_H

#include <map>
#include <list>

template<class KEY_TYPE, class VALUE_TYPE>
class Cache
{
public:
	Cache();
	virtual ~Cache();
	VALUE_TYPE& get(KEY_TYPE& key);
	void add(KEY_TYPE& key, VALUE_TYPE& value);
	bool contains(KEY_TYPE& key) const;
	void setMaxCacheSize(int maxCacheSize);
private:
	int maxCacheSize;
	std::map<KEY_TYPE, VALUE_TYPE> cache;
	std::list<KEY_TYPE> order;
};

#endif
