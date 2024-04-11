#pragma once
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <syncstream>
#include <thread>
#include <variant>
#include <typeinfo>

#include "CacheErrorCodes.h"
#include "IFlushCallback.h"

//template<typename KeyType, template <typename...> typename ValueType, typename... ValueCoreTypes>
template<typename KeyType, typename... ValueCoreTypes>
class NoCache
{
public:
	typedef KeyType ObjectUIDType;
	typedef void ObjectType;
	typedef void* ObjectTypePtr;
	typedef std::tuple<ValueCoreTypes...> ObjectCoreTypes;

public:
	~NoCache()
	{
	}

	NoCache()
	{
	}

	template <typename... InitArgs>
	CacheErrorCode init(InitArgs... args)
	{
		return CacheErrorCode::Success;
	}

	CacheErrorCode remove(ObjectUIDType objKey)
	{
		ObjectTypePtr ptrValue = reinterpret_cast<ObjectTypePtr>(objKey.m_ptrVolatile);
		delete ptrValue;

		return CacheErrorCode::KeyDoesNotExist;
	}

	CacheErrorCode getObject(const ObjectUIDType& objKey, ObjectTypePtr& ptrObject)
	{
		ptrObject = reinterpret_cast<ObjectTypePtr>(objKey.m_ptrVolatile);
		return CacheErrorCode::Success;
	}

	template <typename Type>
	inline CacheErrorCode getObjectOfType(const ObjectUIDType& objKey, Type*& ptrObject)
	{
		ptrObject = reinterpret_cast<Type*>(objKey.m_ptrVolatile);

		return CacheErrorCode::Error;
	}

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& key, ArgsType... args)
	{
		//ObjectTypePtr ptrValue = new std::variant<ValueCoreTypes...>(std::make_shared<Type>(args...));
		
		ObjectTypePtr ptrValue = new Type(args...);

		key = ObjectUIDType::createAddressFromVolatilePointer(Type::UID, reinterpret_cast<uintptr_t>(ptrValue));
		return CacheErrorCode::Success;
	}

	void getCacheState(size_t& lru, size_t& map)
	{
		lru = 1;
		map = 1;
	}

	CacheErrorCode reorder(std::vector<std::pair<ObjectUIDType, ObjectTypePtr>>& vt, bool ensure = true)
	{
		return CacheErrorCode::Success;
	}
};
