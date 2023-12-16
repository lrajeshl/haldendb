#pragma once
#include "ErrorCodes.h"

template <typename KeyType>
class IFlushCallback
{
public:
	virtual CacheErrorCode keyUpdate(KeyType oldKey, KeyType newKey, KeyType parentKey) = 0;
};

