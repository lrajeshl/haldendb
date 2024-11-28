#pragma once
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <syncstream>
#include <thread>
#include <variant>
#include <typeinfo>
#include <unordered_map>
#include <queue>
#include  <algorithm>
#include <tuple>
#include <condition_variable>
#include <assert.h>
#include "IFlushCallback.h"
#include "VariadicNthType.h"

#define FLUSH_COUNT 100
#define MIN_CACHE_FOOTPRINT 1024 * 1024	// Safe check!

using namespace std::chrono_literals;

template <typename ICallback, typename StorageType>
class SSARCCache : public ICallback
{
	typedef SSARCCache<ICallback, StorageType> SelfType;

public:
	typedef StorageType::ObjectUIDType ObjectUIDType;
	typedef StorageType::ObjectType ObjectType;
	typedef std::shared_ptr<ObjectType> ObjectTypePtr;

private:
	struct Item
	{
	public:
		ObjectUIDType m_uidSelf;
		ObjectTypePtr m_ptrObject;
		std::shared_ptr<Item> m_ptrPrev;
		std::shared_ptr<Item> m_ptrNext;

		Item(const ObjectUIDType& uidObject, const ObjectTypePtr ptrObject)
			: m_ptrNext(nullptr)
			, m_ptrPrev(nullptr)
		{
			m_uidSelf = uidObject;
			m_ptrObject = ptrObject;
		}

		~Item()
		{
			m_ptrPrev.reset();
			m_ptrNext.reset();
			m_ptrObject.reset();
		}
	};

	ICallback* m_ptrCallback;

	std::unique_ptr<StorageType> m_ptrStorage;

	int64_t m_nCacheFootprint;
	int64_t m_nCacheCapacity;
	std::unordered_map<ObjectUIDType, std::shared_ptr<Item>> m_mpObjects;

public:
	~SSARCCache()
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		assert(m_nCacheFootprint == 0);
	}

	template <typename... StorageArgs>
	SSARCCache(size_t nCapacity, StorageArgs... args)
		: m_nCacheCapacity(nCapacity)
		, m_nCacheFootprint(0)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");
	}

	void updateMemoryFootprint(int32_t nMemoryFootprint)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");
	}

	template <typename... InitArgs>
	CacheErrorCode init(ICallback* ptrCallback, InitArgs... args)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		return CacheErrorCode::Error;
	}

	CacheErrorCode remove(const ObjectUIDType& uidObject)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		return CacheErrorCode::KeyDoesNotExist;
	}

	CacheErrorCode getObject(const ObjectUIDType& uidObject, ObjectTypePtr& ptrObject, std::optional<ObjectUIDType>& uidUpdated)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		return CacheErrorCode::Error;
	}

	CacheErrorCode reorder(std::vector<std::pair<ObjectUIDType, ObjectTypePtr>>& vt, bool bEnsure = true)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		return CacheErrorCode::Success;
	}

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, const ArgsType... args)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		return CacheErrorCode::Success;
	}

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, ObjectTypePtr& ptrStorageObject, const ArgsType... args)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		return CacheErrorCode::Success;
	}

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, std::shared_ptr<Type>& ptrCoreObject, const ArgsType... args)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		return CacheErrorCode::Success;
	}

	void getCacheState(size_t& nObjectsLinkedList, size_t& nObjectsInMap)
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");
	}

	CacheErrorCode flush()
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");

		return CacheErrorCode::Success;
	}

private:
	inline void flushItemsToStorage()
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");
	}

	inline void flushAllItemsToStorage()
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");
	}

	inline void flushDataItemsToStorage()
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");
	}

	inline void presistCurrentCacheState()
	{
		std::cout << "unimplemented!" << std::endl;
		throw new std::logic_error("unimplemented!");
	}

#ifdef __CONCURRENT__
	static void handlerCacheFlush(SelfType* ptrSelf)
	{
		do
		{
			//std::cout << "thread..." << std::endl;
			ptrSelf->flushItemsToStorage();

			std::this_thread::sleep_for(1ms);

		} while (!ptrSelf->m_bStop);
	}
#endif //__CONCURRENT__

#ifdef __TREE_WITH_CACHE__
public:
	void applyExistingUpdates(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
		, std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUpdatedUIDs)
	{
	}

	void applyExistingUpdates(std::shared_ptr<ObjectType> ptrObject
		, std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUpdatedUIDs)
	{
	}

	void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
		, size_t nOffset, size_t& nNewOffset, size_t nBlockSize, ObjectUIDType::StorageMedia nMediaType)
	{
	}
#endif //__TREE_WITH_CACHE__
};
