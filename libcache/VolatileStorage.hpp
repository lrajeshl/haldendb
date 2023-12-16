#pragma once
#include <memory>
#include <unordered_map>

#include "UnsortedMapUtil.h"
#include "ErrorCodes.h"

template <
	typename ICallback,
	typename KeyType, 
	template <typename, typename...> typename ValueType, 
	typename ValueCoreTypesMarshaller, 
	typename... ValueCoreTypes>
class VolatileStorage
{
public:
	typedef KeyType CacheKeyType;
	typedef ValueType<ValueCoreTypesMarshaller, ValueCoreTypes...> CacheValueType;

	typedef std::shared_ptr<ValueType<ValueCoreTypesMarshaller, ValueCoreTypes...>> CacheKeyTypePtr;
private:
	size_t m_nPoolSize;
	std::unordered_map<KeyType, CacheKeyTypePtr> m_mpObject;

public:
	VolatileStorage(size_t nPoolSize)
		: m_nPoolSize(nPoolSize)
	{
	}

	CacheKeyTypePtr getObject(KeyType ptrKey)
	{
		if (m_mpObject.find(ptrKey) != m_mpObject.end())
		{
			return m_mpObject[ptrKey];
		}

		return nullptr;
	}

	CacheErrorCode addObject(KeyType ptrKey, CacheKeyTypePtr ptrValue)
	{
		if (m_mpObject.size() >= m_nPoolSize)
		{
			return CacheErrorCode::Error;
		}

		m_mpObject[ptrKey] = ptrValue;

		return CacheErrorCode::Success;
	}
};

