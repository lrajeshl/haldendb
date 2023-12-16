#pragma once
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <syncstream>
#include <thread>
#include <variant>
#include <typeinfo>

#include "ErrorCodes.h"
#include "IFlushCallback.h"

template<typename KeyType, template <typename...> typename ValueType, typename... ValueCoreTypes>
class NoCache
{
public:
	typedef KeyType CacheKeyType;
	typedef ValueType<ValueCoreTypes...> CacheValueType;
	
	//typedef KeyType KeyType;
	//typedef ValueType<ValueCoreTypes...>* CacheValueType;

	typedef ValueType<ValueCoreTypes...>* CacheValueTypePtr;

private:

public:
	~NoCache()
	{
	}

	NoCache()
	{
	}

	CacheErrorCode init(IFlushCallback<KeyType> ptrCallback)
	{
		return CacheErrorCode::Success;
	}


	CacheErrorCode remove(KeyType objKey)
	{
		CacheValueTypePtr ptrValue = reinterpret_cast<CacheValueTypePtr>(objKey);
		delete ptrValue;

		return CacheErrorCode::KeyDoesNotExist;
	}

	std::shared_ptr<CacheValueType> getObject(KeyType objKey)
	{
		CacheValueTypePtr ptr = reinterpret_cast<CacheValueTypePtr>(objKey);
		return std::shared_ptr<CacheValueType>(ptr);
	}

	template <typename Type>
	Type getObjectOfType(KeyType objKey)
	{
		CacheValueTypePtr ptrValue = reinterpret_cast<CacheValueTypePtr>(objKey);

		if (std::holds_alternative<Type>(*ptrValue->data))
		{
			return std::get<Type>(*ptrValue->data);
		}

		return nullptr;
	}

	template<class Type, typename... ArgsType>
	KeyType createObjectOfType(ArgsType... args)
	{
		//CacheValueTypePtr ptrValue = new std::variant<ValueCoreTypes...>(std::make_shared<Type>(args...));
		
		ValueType<ValueCoreTypes...>* ptrValue = ValueType<ValueCoreTypes...>::template createObjectOfType<Type>(args...);

		return reinterpret_cast<KeyType>(ptrValue);
	}
};
