#pragma once
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <syncstream>
#include <thread>
#include <variant>
#include <typeinfo>

#include "ErrorCodes.h"

template <typename... ValueCoreTypes>
class NoCacheObject
{
private:
	typedef std::variant<std::shared_ptr<ValueCoreTypes>...> ValueCoreTypesWrapper;

public:
	typedef std::tuple<ValueCoreTypes...> ValueCoreTypesTuple;

public:
	ValueCoreTypesWrapper data;
	mutable std::shared_mutex mutex;

public:
	template<class CoreObjectType>
	NoCacheObject(std::shared_ptr<CoreObjectType> ptrCoreObject)
	{
		data = ptrCoreObject;
	}
};