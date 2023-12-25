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

#include "ErrorCodes.h"
#include "IFlushCallback.h"
#include "VariadicNthType.h"

#define __CONCURRENT__

template <typename ICallback, typename StorageType>
class LRUCache
{
	typedef LRUCache<ICallback, StorageType> SelfType;

public:
	typedef StorageType::ObjectUIDType ObjectUIDType;
	typedef StorageType::ObjectType ObjectType;
	typedef std::shared_ptr<ObjectType> ObjectTypePtr;

private:
	struct Item
	{
	public:
		ObjectUIDType m_oKey;
		ObjectTypePtr m_ptrValue;
		std::shared_ptr<Item> m_ptrPrev;
		std::shared_ptr<Item> m_ptrNext;

		Item(ObjectUIDType& key, ObjectTypePtr ptrValue)
			: m_ptrNext(nullptr)
			, m_ptrPrev(nullptr)
		{
			m_oKey = key;
			m_ptrValue = ptrValue;
		}

		~Item()
		{
			m_ptrPrev = nullptr;
			m_ptrNext = nullptr;
			m_ptrValue = nullptr;
		}
	};

	std::unordered_map<ObjectUIDType, std::shared_ptr<Item>> m_mpObject;

	size_t m_nCapacity;
	std::shared_ptr<Item> m_ptrHead;
	std::shared_ptr<Item> m_ptrTail;

#ifdef __CONCURRENT__
	bool m_bStop;

	std::thread m_threadCacheFlush;

	mutable std::shared_mutex m_mtxCache;
	mutable std::shared_mutex m_mtxStorage;
#endif __CONCURRENT__

	std::unique_ptr<StorageType> m_ptrStorage;

public:
	~LRUCache()
	{
#ifdef __CONCURRENT__
		m_bStop = true;
		m_threadCacheFlush.join();
#endif __CONCURRENT__

		m_ptrHead = nullptr;
		m_ptrTail = nullptr;
		m_ptrStorage = nullptr;

		m_mpObject.clear();
	}

	template <typename... StorageArgs>
	LRUCache(size_t nCapacity, StorageArgs... args)
		: m_nCapacity(nCapacity)
		, m_ptrHead(nullptr)
		, m_ptrTail(nullptr)
	{
		m_ptrStorage = std::make_unique<StorageType>(args...);

#ifdef __CONCURRENT__
		m_bStop = false;
		m_threadCacheFlush = std::thread(handlerCacheFlush, this);
#endif __CONCURRENT__
	}

	template <typename... InitArgs>
	CacheErrorCode init(InitArgs... args)
	{	
		//m_ptrCallback = ptrCallback;
		return m_ptrStorage->init(getNthElement<0>(args...));
	}

	CacheErrorCode remove(ObjectUIDType key)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex>  lock_cache(m_mtxCache);
#endif __CONCURRENT__

		auto it = m_mpObject.find(key);
		if (it != m_mpObject.end()) 
		{
			removeFromLRU((*it).second);
			m_mpObject.erase((*it).first);
			return CacheErrorCode::Success;
		}

		CacheErrorCode errCode = m_ptrStorage->remove(key);
		if (errCode != CacheErrorCode::Success)
		{
			throw new std::exception("should not occur!");
		}

		return CacheErrorCode::Success;
	}

	CacheErrorCode getObject(ObjectUIDType key, ObjectTypePtr& ptrObject)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache); // std::unique_lock due to LRU's linked-list update! is there any better way?
#endif __CONCURRENT__

		if (m_mpObject.find(key) != m_mpObject.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObject[key];
			moveToFront(ptrItem);
			ptrObject = ptrItem->m_ptrValue;
			return CacheErrorCode::Success;
		}

#ifdef __CONCURRENT__
		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage); // TODO: requesting the same key?
		lock_cache.unlock();
#endif __CONCURRENT__

		std::shared_ptr<ObjectType> ptrValue = m_ptrStorage->getObject(key);

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		if (ptrValue != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(key, ptrValue);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);
#endif __CONCURRENT__

			if (m_mpObject.find(key) != m_mpObject.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObject[key];
				moveToFront(ptrItem);
				ptrObject = ptrItem->m_ptrValue;
				return CacheErrorCode::Success;
			}

			m_mpObject[key] = ptrItem;

			if (!m_ptrHead) 
			{
				m_ptrHead = ptrItem;
				m_ptrTail = ptrItem;
			}
			else 
			{
				ptrItem->m_ptrNext = m_ptrHead;
				m_ptrHead->m_ptrPrev = ptrItem;
				m_ptrHead = ptrItem;
			}

			ptrObject = ptrValue;
#ifndef __CONCURRENT__
			flushItemsToStorage();
#endif __CONCURRENT__
			return CacheErrorCode::Success;
		}
		
		return CacheErrorCode::Error;
	}

	template <typename Type>
	CacheErrorCode getObjectOfType(ObjectUIDType key, Type& ptrObject)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObject.find(key) != m_mpObject.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObject[key];
			moveToFront(ptrItem);

//#ifdef __CONCURRENT__
//			lock_cache.unlock();
//#endif __CONCURRENT__

			if (std::holds_alternative<Type>(*ptrItem->m_ptrValue->data))
			{
				ptrObject = std::get<Type>(*ptrItem->m_ptrValue->data);
				return CacheErrorCode::Success;
			}

			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage);
		lock_cache.unlock();
#endif __CONCURRENT__

		std::shared_ptr<ObjectType> ptrValue = m_ptrStorage->getObject(key);

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		if (ptrValue != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(key, ptrValue);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);
#endif __CONCURRENT__


			if (m_mpObject.find(key) != m_mpObject.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObject[key];
				moveToFront(ptrItem);

				if (std::holds_alternative<Type>(*ptrItem->m_ptrValue->data))
				{
					ptrObject = std::get<Type>(*ptrItem->m_ptrValue->data);
					return CacheErrorCode::Success;
				}

				return CacheErrorCode::Error;
			}





			m_mpObject[key] = ptrItem;

			if (!m_ptrHead)
			{
				m_ptrHead = ptrItem;
				m_ptrTail = ptrItem;
			}
			else
			{
				ptrItem->m_ptrNext = m_ptrHead;
				m_ptrHead->m_ptrPrev = ptrItem;
				m_ptrHead = ptrItem;
			}

//#ifdef __CONCURRENT__
//			lock_cache.unlock();
//#endif __CONCURRENT__

			if (std::holds_alternative<Type>(*ptrValue->data))
			{
				ptrObject = std::get<Type>(*ptrValue->data);
				return CacheErrorCode::Success;
			}

#ifndef __CONCURRENT__
			flushItemsToStorage();
#endif __CONCURRENT__
			return CacheErrorCode::Error;
		}

		return CacheErrorCode::Error;
	}

	template<class Type, typename... ArgsType>
	ObjectUIDType createObjectOfType(ArgsType... args)
	{
		ObjectUIDType key;
		std::shared_ptr<ObjectType> ptrValue = ObjectType::template createObjectOfType<Type>(args...);

		key = ObjectUIDType::createAddressFromVolatilePointer(reinterpret_cast<uintptr_t>(ptrValue.get()));

		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(key, ptrValue);

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObject.find(key) != m_mpObject.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObject[key];
			ptrItem->m_ptrValue = ptrValue;
			moveToFront(ptrItem);
		}
		else
		{
			m_mpObject[key] = ptrItem;
			if (!m_ptrHead) 
			{
				m_ptrHead = ptrItem;
				m_ptrTail = ptrItem;
			}
			else 
			{
				ptrItem->m_ptrNext = m_ptrHead;
				m_ptrHead->m_ptrPrev = ptrItem;
				m_ptrHead = ptrItem;
			}
		}

#ifdef __CONCURRENT__
		//..
#else
		flushItemsToStorage();
#endif __CONCURRENT__

		return key;
	}

	int getlrucount()
	{
		int cnt = 0;
		std::shared_ptr<Item> _ptrItem = m_ptrHead;
		do
		{
			cnt++;
			_ptrItem = _ptrItem->m_ptrNext;

		} while (_ptrItem != nullptr);
		return cnt;
	}
private:
	inline void moveToFront(std::shared_ptr<Item> ptrItem)
	{
		if (ptrItem == m_ptrHead)
		{
			//m_ptrHead->m_ptrPrev = nullptr;
			return;
		}

		if (ptrItem->m_ptrPrev) {
			ptrItem->m_ptrPrev->m_ptrNext = ptrItem->m_ptrNext;
		}

		if (ptrItem->m_ptrNext) {
			ptrItem->m_ptrNext->m_ptrPrev = ptrItem->m_ptrPrev;
		}

		if (ptrItem == m_ptrTail) {
			m_ptrTail = ptrItem->m_ptrPrev;
		}

		ptrItem->m_ptrPrev = nullptr;
		ptrItem->m_ptrNext = m_ptrHead;

		if (m_ptrHead) {
			m_ptrHead->m_ptrPrev = ptrItem;
		}
		m_ptrHead = ptrItem;
	}

	inline void removeFromLRU(std::shared_ptr<Item> ptrItem)
	{
		if (ptrItem->m_ptrPrev != nullptr) {
			ptrItem->m_ptrPrev->m_ptrNext = ptrItem->m_ptrNext;
		}
		else {
			// If the node to be removed is the head
			m_ptrHead = ptrItem->m_ptrNext;
			if (m_ptrHead != nullptr)
			m_ptrHead->m_ptrPrev = nullptr;

		}

		if (ptrItem->m_ptrNext != nullptr) {
			ptrItem->m_ptrNext->m_ptrPrev = ptrItem->m_ptrPrev;
		}
		else {
			// If the node to be removed is the tail
			m_ptrTail = ptrItem->m_ptrPrev;
			if (m_ptrTail != nullptr)

				m_ptrTail->m_ptrNext = nullptr;
		}


		/*if (ptrItem == m_ptrHead)
		{
			m_ptrHead = ptrItem->m_ptrNext;
			m_ptrHead->m_ptrPrev = nullptr;
		}

		if (ptrItem == m_ptrTail)
		{
			m_ptrTail = ptrItem->m_ptrPrev;
			m_ptrTail->m_ptrNext = nullptr;
		}

		if (ptrItem->m_ptrPrev != nullptr && ptrItem->m_ptrNext != nullptr)
		{
			ptrItem->m_ptrPrev->m_ptrNext = ptrItem->m_ptrNext;
			ptrItem->m_ptrNext->m_ptrPrev = ptrItem->m_ptrPrev;
		}

		ptrItem->m_ptrPrev = nullptr;
		ptrItem->m_ptrNext = nullptr;*/
	}

	inline void flushItemsToStorage()
	{
#ifdef __CONCURRENT__

		std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtItems;

		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);

		if (m_mpObject.size() < m_nCapacity)
			return;

		size_t nFlushCount = m_mpObject.size() - m_nCapacity;
		for (size_t idx = 0; idx < nFlushCount; idx++)
		{
			if (!m_ptrTail->m_ptrValue->mutex.try_lock())
			{
				// in use.. TODO: proceed with the next one.
				break;
			}
			m_ptrTail->m_ptrValue->mutex.unlock();


			std::shared_ptr<Item> ptrTemp = m_ptrTail;

			vtItems.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(ptrTemp->m_oKey, ptrTemp->m_ptrValue));

			m_mpObject.erase(ptrTemp->m_oKey);

			m_ptrTail = ptrTemp->m_ptrPrev;

			ptrTemp->m_ptrPrev = nullptr;
			ptrTemp->m_ptrNext = nullptr;

			if (m_ptrTail)
			{
				m_ptrTail->m_ptrNext = nullptr;
			}
			else
			{
				m_ptrHead = nullptr;
			}
		}

		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);

		lock_cache.unlock();

		auto it = vtItems.begin();
		while (it != vtItems.end())
		{
			m_ptrStorage->addObject((*it).first, (*it).second);

			if ((*it).second.use_count() != 2)
			{
				throw new std::exception("should not occur!");
			}

			it++;
		}

		vtItems.clear();
#else
		while (m_mpObject.size() >= m_nCapacity)
		{
			m_ptrStorage->addObject(m_ptrTail->m_oKey, m_ptrTail->m_ptrValue);
			m_mpObject.erase(m_ptrTail->m_oKey);

			std::shared_ptr<Item> ptrTemp = m_ptrTail;
			m_ptrTail = m_ptrTail->m_ptrPrev;
			if (m_ptrTail)
			{
				m_ptrTail->m_ptrNext = nullptr;
			}
			else
			{
				m_ptrHead = nullptr;
			}
		}
#endif __CONCURRENT__
	}

#ifdef __CONCURRENT__
	static void handlerCacheFlush(SelfType* ptrSelf)
	{
		do
		{
			ptrSelf->flushItemsToStorage();

			std::this_thread::sleep_for(100ms);

		} while (!ptrSelf->m_bStop);
	}
#endif __CONCURRENT__
};