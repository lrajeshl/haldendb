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

private:
	ValueCoreTypesWrapper m_objData;
	mutable std::shared_mutex m_mtx;

public:
	template<class CoreObjectType>
	NoCacheObject(std::shared_ptr<CoreObjectType> ptrCoreObject)
	{
		m_objData = ptrCoreObject;
	}

	inline const ValueCoreTypesWrapper& getInnerData() const
	{
		return m_objData;
	}

	inline std::shared_mutex& getMutex() const
	{
		return m_mtx;
	}

	inline bool tryLockObject() const
	{
		return m_mtx.try_lock();
	}

	inline void unlockObject() const
	{
		m_mtx.unlock();
	}
};