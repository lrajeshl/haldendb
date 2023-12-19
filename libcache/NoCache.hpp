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

template<typename KeyType, template <typename...> typename ValueType, typename... ValueCoreTypes>
class NoCache
{
public:
	typedef KeyType ObjectUIDType;
	typedef ValueType<ValueCoreTypes...> ObjectType;
	typedef ValueType<ValueCoreTypes...>* ObjectTypePtr;
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
		ObjectTypePtr ptrValue = reinterpret_cast<ObjectTypePtr>(objKey);
		delete ptrValue;

		return CacheErrorCode::KeyDoesNotExist;
	}

	ObjectTypePtr getObject(ObjectUIDType objKey)
	{
		return reinterpret_cast<ObjectTypePtr>(objKey);
	}

	template <typename Type>
	Type getObjectOfType(ObjectUIDType objKey)
	{
		ObjectTypePtr ptrValue = reinterpret_cast<ObjectTypePtr>(objKey);

		if (std::holds_alternative<Type>(*ptrValue->data))
		{
			return std::get<Type>(*ptrValue->data);
		}

		return nullptr;
	}

	template<class Type, typename... ArgsType>
	ObjectUIDType createObjectOfType(ArgsType... args)
	{
		//ObjectTypePtr ptrValue = new std::variant<ValueCoreTypes...>(std::make_shared<Type>(args...));
		
		ValueType<ValueCoreTypes...>* ptrValue = ValueType<ValueCoreTypes...>::template createObjectOfType<Type>(args...);

		return reinterpret_cast<ObjectUIDType>(ptrValue);
	}
};
