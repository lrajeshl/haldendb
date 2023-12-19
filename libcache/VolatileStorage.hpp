#pragma once
#include <memory>
#include <unordered_map>

#include "ErrorCodes.h"

template<
	typename ICallback,
	typename ObjectUIDType, 
	template <typename, typename...> typename ObjectWrapperType, 
	typename ObjectTypeMarshaller, 
	typename... ObjectTypes>
class VolatileStorage
{
public:
	typedef ObjectUIDType ObjectUIDType;
	typedef ObjectWrapperType<ObjectTypeMarshaller, ObjectTypes...> ObjectType;
	typedef std::tuple<ObjectTypes...> ObjectCoreTypes;

private:
	size_t m_nPoolSize;
	std::unordered_map<ObjectUIDType, std::shared_ptr<ObjectType>> m_mpObject;

	ICallback* m_ptrCallback;

public:
	VolatileStorage(size_t nPoolSize)
		: m_nPoolSize(nPoolSize)
	{
	}

	template <typename... InitArgs>
	CacheErrorCode init(InitArgs... args)
	{
		m_ptrCallback = getNthElement<0>(args...);
		return CacheErrorCode::Success;
	}

	std::shared_ptr<ObjectType> getObject(ObjectUIDType ptrKey)
	{
		if (m_mpObject.find(ptrKey) != m_mpObject.end())
		{
			return m_mpObject[ptrKey];
		}

		return nullptr;
	}

	CacheErrorCode addObject(ObjectUIDType ptrKey, std::shared_ptr<ObjectType> ptrValue)
	{
		if (m_mpObject.size() >= m_nPoolSize)
		{
			return CacheErrorCode::Error;
		}

		m_mpObject[ptrKey] = ptrValue;

		return CacheErrorCode::Success;
	}
};

