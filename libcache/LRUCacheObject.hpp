#pragma once
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <syncstream>
#include <thread>
#include <variant>
#include <typeinfo>

#include "ErrorCodes.h"

template <typename ValueCoreTypesMarshaller, typename... ValueCoreTypes>
class LRUCacheObject
{
	typedef std::variant<ValueCoreTypes...> CacheValueType_;
	typedef std::variant<std::shared_ptr<ValueCoreTypes>...> CacheValueType;
	typedef std::shared_ptr<std::variant<std::shared_ptr<ValueCoreTypes>...>> CacheValueTypePtr;

public:
	CacheValueTypePtr data;
	mutable std::shared_mutex mutex;

public:
	LRUCacheObject(CacheValueTypePtr ptrValue)
	{
		data = ptrValue;
	}

	template<class Type, typename... ArgsType>
	static std::shared_ptr<LRUCacheObject> createObjectOfType(ArgsType... args)
	{
		CacheValueTypePtr ptrValue = std::make_shared<CacheValueType>(std::make_shared<Type>(args...));

		return std::make_shared<LRUCacheObject>(ptrValue);
	}

	template<class Type, typename... ArgsType>
	static std::shared_ptr<LRUCacheObject> createObjectOfType(std::shared_ptr<Type>& ptrCoreValue, ArgsType... args)
	{
		ptrCoreValue = std::make_shared<Type>(args...);
		CacheValueTypePtr ptrValue = std::make_shared<CacheValueType>(ptrCoreValue);

		return std::make_shared<LRUCacheObject>(ptrValue);
	}

	std::tuple<uint8_t, const std::byte*, size_t> serialize()
	{
		return ValueCoreTypesMarshaller::template serialize<ValueCoreTypes...>(*data);
	}

	void deserialize(uint8_t uid, std::byte* bytes) 
	{

		CacheValueType_ deserializedVariant;

		ValueCoreTypesMarshaller::template deserialize<ValueCoreTypes...>(uid, bytes, deserializedVariant);

	}

};
