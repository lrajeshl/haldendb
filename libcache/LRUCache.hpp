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

#include "ErrorCodes.h"
#include "IFlushCallback.h"
#include "VariadicNthType.h"

#define __CONCURRENT__
#define __POSITION_AWARE_ITEMS__

bool isEventSignaled = false;


#ifdef __POSITION_AWARE_ITEMS__
template <typename ICallback, typename StorageType>
class LRUCache : public ICallback
#else //__POSITION_AWARE_ITEMS__
template <typename StorageType>
class LRUCache
#endif __POSITION_AWARE_ITEMS__
{

#ifdef __POSITION_AWARE_ITEMS__
	typedef LRUCache<ICallback, StorageType> SelfType;
#else //__POSITION_AWARE_ITEMS__
	typedef LRUCache<StorageType> SelfType;
#endif __POSITION_AWARE_ITEMS__

public:
	typedef StorageType::ObjectUIDType ObjectUIDType;
	typedef StorageType::ObjectType ObjectType;
	typedef std::shared_ptr<ObjectType> ObjectTypePtr;

private:
	struct Item
	{
	public:
#ifdef __POSITION_AWARE_ITEMS__
		std::optional<ObjectUIDType> m_uidParent;
#endif __POSITION_AWARE_ITEMS__

		ObjectUIDType m_uidSelf;
		ObjectTypePtr m_ptrObject;
		std::shared_ptr<Item> m_ptrPrev;
		std::shared_ptr<Item> m_ptrNext;

#ifdef __POSITION_AWARE_ITEMS__
		Item(const ObjectUIDType& key, const ObjectTypePtr ptrObject, const std::optional<ObjectUIDType>& uidParent)
			: m_ptrNext(nullptr)
			, m_ptrPrev(nullptr)
		{
			m_uidSelf = key;
			m_ptrObject = ptrObject;

			m_uidParent = uidParent;
		}
#endif __POSITION_AWARE_ITEMS__

		Item(const ObjectUIDType& key, const ObjectTypePtr ptrObject)
			: m_ptrNext(nullptr)
			, m_ptrPrev(nullptr)
		{
			m_uidSelf = key;
			m_ptrObject = ptrObject;
		}

		~Item()
		{
			m_ptrPrev = nullptr;
			m_ptrNext = nullptr;
			m_ptrObject = nullptr;
		}
	};



#ifdef __POSITION_AWARE_ITEMS__
	ICallback* m_ptrCallback;
#endif __POSITION_AWARE_ITEMS__

	std::shared_ptr<Item> m_ptrHead;
	std::shared_ptr<Item> m_ptrTail;

	std::unique_ptr<StorageType> m_ptrStorage;

	size_t m_nCacheCapacity;
	std::unordered_map<ObjectUIDType, std::shared_ptr<Item>> m_mpObjects;

	//std::mutex m_mtxUIDUpdates;
	//std::condition_variable_any cv;


	std::mutex mutexForEvent;
	std::condition_variable_any cv;
	std::unordered_map<ObjectUIDType, std::shared_ptr<UIDUpdate>> m_mpDepartureQueue;

#ifdef __CONCURRENT__
	bool m_bStop;

	std::thread m_threadCacheFlush;

	mutable std::shared_mutex m_mtxCache;
	mutable std::shared_mutex m_mtxStorage;
#endif __CONCURRENT__

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

		m_mpObjects.clear();
	}

	template <typename... StorageArgs>
	LRUCache(size_t nCapacity, StorageArgs... args)
		: m_nCacheCapacity(nCapacity)
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
#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode init(ICallback* ptrCallback, InitArgs... args)
#else __POSITION_AWARE_ITEMS__
	CacheErrorCode init(InitArgs... args)
#endif  __POSITION_AWARE_ITEMS__
	{
#ifdef __POSITION_AWARE_ITEMS__

#ifdef __CONCURRENT__
		m_ptrCallback = ptrCallback;
		return m_ptrStorage->init(this/*getNthElement<0>(args...)*/);
#else // ! __CONCURRENT__
		return m_ptrStorage->init(ptrCallback/*getNthElement<0>(args...)*/);
#endif __CONCURRENT__

#else // !__POSITION_AWARE_ITEMS__
		//return m_ptrStorage->template init<InitArgs>(args...);
		return m_ptrStorage->init();
#endif __POSITION_AWARE_ITEMS__
	}

	CacheErrorCode remove(const ObjectUIDType& uidObject)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex>  lock_cache(m_mtxCache);
#endif __CONCURRENT__

		auto it = m_mpObjects.find(uidObject);
		if (it != m_mpObjects.end()) 
		{
			removeFromLRU((*it).second);
			m_mpObjects.erase((*it).first);
			return CacheErrorCode::Success;
		}

		return m_ptrStorage->remove(uidObject);
	}

#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode getObject(const ObjectUIDType& uidObject, ObjectTypePtr& ptrObject, std::optional<ObjectUIDType>& uidParent, bool bCacheOnly = false)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache); // std::unique_lock due to LRU's linked-list update! is there any better way?
#endif __CONCURRENT__

		if (m_mpObjects.find(uidObject) != m_mpObjects.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObjects[uidObject];
			moveToFront(ptrItem);
			ptrObject = ptrItem->m_ptrObject;
			uidParent = ptrItem->m_uidParent;
			return CacheErrorCode::Success;
		}

		if (bCacheOnly)
		{
			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage); // TODO: requesting the same key?
		lock_cache.unlock();
#endif __CONCURRENT__

		ObjectUIDType _uidUpdateUID = uidObject;
		if (m_mpDepartureQueue.find(uidObject) != m_mpDepartureQueue.end())
		{
			while (m_mpDepartureQueue[uidObject]->uidUpdated == std::nullopt)
			{
				cv.wait(lock_storage, [] { return isEventSignaled; });
			}
			_uidUpdateUID = *m_mpDepartureQueue[uidObject]->uidUpdated;
		}

		//newuid = _uidUpdateUID;
		// use uidUpdateUID onwards!
		// set new uid..

		std::shared_ptr<ObjectType> _ptrObject = m_ptrStorage->getObject(_uidUpdateUID);


#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		if (_ptrObject != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(_uidUpdateUID, _ptrObject, uidParent);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(_uidUpdateUID) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[_uidUpdateUID];
				moveToFront(ptrItem);
				ptrObject = ptrItem->m_ptrObject;
				uidParent = ptrItem->m_uidParent;
				return CacheErrorCode::Success;
			}
#endif __CONCURRENT__

			m_mpObjects[_uidUpdateUID] = ptrItem;

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

			ptrObject = _ptrObject;

#ifndef __CONCURRENT__
			flushItemsToStorage();
#endif __CONCURRENT__

			return CacheErrorCode::Success;
		}
		
		return CacheErrorCode::Error;
	}
#endif __POSITION_AWARE_ITEMS__

	CacheErrorCode getObject(const ObjectUIDType uidObject, ObjectTypePtr & ptrObject, bool bCacheOnly = false)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache); // std::unique_lock due to LRU's linked-list update! is there any better way?
#endif __CONCURRENT__

		if (m_mpObjects.find(uidObject) != m_mpObjects.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObjects[uidObject];
			moveToFront(ptrItem);
			ptrObject = ptrItem->m_ptrObject;
			return CacheErrorCode::Success;
		}

		if (bCacheOnly)
		{
			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage); // TODO: requesting the same key?
		lock_cache.unlock();
#endif __CONCURRENT__

		ObjectUIDType _uidUpdateUID = uidObject;
		if (m_mpDepartureQueue.find(uidObject) != m_mpDepartureQueue.end())
		{
			cv.wait(m_mtxStorage, [] { return isEventSignaled; });
			_uidUpdateUID = *m_mpDepartureQueue[uidObject]->uidUpdated;
		}

		//newuid = _uidUpdateUID;

		std::shared_ptr<ObjectType> _ptrObject = m_ptrStorage->getObject(_uidUpdateUID);

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		if (_ptrObject != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(_uidUpdateUID, _ptrObject);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(_uidUpdateUID) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[_uidUpdateUID];
				moveToFront(ptrItem);
				ptrObject = ptrItem->m_ptrObject;
				return CacheErrorCode::Success;
			}
#endif __CONCURRENT__

			m_mpObjects[_uidUpdateUID] = ptrItem;

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

			ptrObject = _ptrObject;

#ifndef __CONCURRENT__
			flushItemsToStorage();
#endif __CONCURRENT__

			return CacheErrorCode::Success;
		}

		return CacheErrorCode::Error;
	}

#ifdef __POSITION_AWARE_ITEMS__
	template <typename Type>
	CacheErrorCode getObjectOfType(const ObjectUIDType uidObject, Type& ptrObject, std::optional<ObjectUIDType>& uidParent)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(uidObject) != m_mpObjects.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObjects[uidObject];
			ptrItem->m_ptrObject->dirty = true;
			moveToFront(ptrItem);

//#ifdef __CONCURRENT__
//			lock_cache.unlock();
//#endif __CONCURRENT__

			if (std::holds_alternative<Type>(*ptrItem->m_ptrObject->data))
			{
				ptrObject = std::get<Type>(*ptrItem->m_ptrObject->data);
				uidParent = ptrItem->m_uidParent;
				return CacheErrorCode::Success;
			}

			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage);
		lock_cache.unlock();
#endif __CONCURRENT__

		ObjectUIDType _uidUpdateUID = uidObject;
		if (m_mpDepartureQueue.find(uidObject) != m_mpDepartureQueue.end())
		{
			cv.wait(m_mtxStorage, [] { return isEventSignaled; });
			_uidUpdateUID = *m_mpDepartureQueue[uidObject]->uidUpdated;
		}

		//newuid = _uidUpdateUID;


		std::shared_ptr<ObjectType> ptrValue = m_ptrStorage->getObject(_uidUpdateUID);

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		if (ptrValue != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(_uidUpdateUID, ptrValue, uidParent);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(_uidUpdateUID) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[_uidUpdateUID];
				moveToFront(ptrItem);

				if (std::holds_alternative<Type>(*ptrItem->m_ptrObject->data))
				{
					ptrObject = std::get<Type>(*ptrItem->m_ptrObject->data);
					uidParent = ptrItem->m_uidParent;
					return CacheErrorCode::Success;
				}

				return CacheErrorCode::Error;
			}

#endif __CONCURRENT__

			m_mpObjects[_uidUpdateUID] = ptrItem;

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
#endif __POSITION_AWARE_ITEMS__

	template <typename Type>
	CacheErrorCode getObjectOfType(const ObjectUIDType uidObject, Type& ptrObject)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(uidObject) != m_mpObjects.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObjects[uidObject];

#ifdef __POSITION_AWARE_ITEMS__
			ptrItem->m_ptrObject->dirty = true;
#endif __POSITION_AWARE_ITEMS__

			moveToFront(ptrItem);

//#ifdef __CONCURRENT__
//			lock_cache.unlock();
//#endif __CONCURRENT__

			if (std::holds_alternative<Type>(*ptrItem->m_ptrObject->data))
			{
				ptrObject = std::get<Type>(*ptrItem->m_ptrObject->data);
				return CacheErrorCode::Success;
			}

			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage);
		lock_cache.unlock();
#endif __CONCURRENT__

		ObjectUIDType _uidUpdateUID = uidObject;
		if (m_mpDepartureQueue.find(uidObject) != m_mpDepartureQueue.end())
		{
			cv.wait(m_mtxStorage, [] { return isEventSignaled; });
			_uidUpdateUID = *m_mpDepartureQueue[uidObject]->uidUpdated;
		}

		//newuid = _uidUpdateUID;


		std::shared_ptr<ObjectType> ptrValue = m_ptrStorage->getObject(_uidUpdateUID);

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		if (ptrValue != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(_uidUpdateUID, ptrValue);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(_uidUpdateUID) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[_uidUpdateUID];
				moveToFront(ptrItem);

				if (std::holds_alternative<Type>(*ptrItem->m_ptrObject->data))
				{
					ptrObject = std::get<Type>(*ptrItem->m_ptrObject->data);
					return CacheErrorCode::Success;
				}

				return CacheErrorCode::Error;
			}
#endif __CONCURRENT__

			m_mpObjects[_uidUpdateUID] = ptrItem;

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
#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, const std::optional<ObjectUIDType>& uidParent, const ArgsType... args)
#else
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, const ArgsType... args)
#endif __POSITION_AWARE_ITEMS__
	{
		std::shared_ptr<ObjectType> ptrValue = ObjectType::template createObjectOfType<Type>(args...);

		uidObject = ObjectUIDType::createAddressFromVolatilePointer(reinterpret_cast<uintptr_t>(ptrValue.get()));

#ifdef __POSITION_AWARE_ITEMS__
		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrValue, uidParent);
#else
		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrValue);
#endif __POSITION_AWARE_ITEMS__

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(*uidObject) != m_mpObjects.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObjects[*uidObject];
			ptrItem->m_ptrObject = ptrValue;
			moveToFront(ptrItem);
		}
		else
		{
			m_mpObjects[*uidObject] = ptrItem;
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

		return CacheErrorCode::Success;
	}

	template<class Type, typename... ArgsType>
#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, const std::optional<ObjectUIDType>& uidParent, std::shared_ptr<Type>& ptrCoreObject, const ArgsType... args)
#else
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, std::shared_ptr<Type>& ptrCoreObject, const ArgsType... args)
#endif __POSITION_AWARE_ITEMS__
	{
		std::shared_ptr<ObjectType> ptrObject = ObjectType::template createObjectOfType<Type>(ptrCoreObject, args...);

		uidObject = ObjectUIDType::createAddressFromVolatilePointer(reinterpret_cast<uintptr_t>(ptrObject.get()));

#ifdef __POSITION_AWARE_ITEMS__
		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrObject, uidParent);
#else
		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrObject);
#endif __POSITION_AWARE_ITEMS__

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(*uidObject) != m_mpObjects.end())
		{
			//TODO: what if the same address is allocated to new object!! fix!! have an extra counter for such a case!
			std::shared_ptr<Item> ptrItem = m_mpObjects[*uidObject];
			ptrItem->m_ptrObject = ptrObject;
			moveToFront(ptrItem);
		}
		else
		{
			m_mpObjects[*uidObject] = ptrItem;
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

		return CacheErrorCode::Success;
	}

#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode tryUpdateParentUID(const ObjectUIDType& uidChild, const std::optional<ObjectUIDType>& uidParent)
	{
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);

		if (m_mpObjects.find(uidChild) != m_mpObjects.end())
		{
			m_mpObjects[uidChild]->m_uidParent = uidParent;
			m_mpObjects[uidChild]->m_ptrObject->dirty = true;
			return CacheErrorCode::Success;
		}

		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage);
		lock_cache.unlock();

		if (m_mpDepartureQueue.find(uidChild) != m_mpDepartureQueue.end())
		{
			bool found = false;
			std::shared_ptr<UIDUpdate> ptrUpdateEntryParent = m_mpDepartureQueue[*uidParent];
			auto it = ptrUpdateEntryParent->vtChildUpdates.begin();
			while (it != ptrUpdateEntryParent->vtChildUpdates.end())
			{
				if ((*it).first == uidChild) {
					found = true;
					ptrUpdateEntryParent->vtChildUpdates.erase(it);
					break;
				}
			}

			assert(found == true);

			// update parent first..
			if (m_mpDepartureQueue.find(*uidParent) != m_mpDepartureQueue.end())
			{
				m_mpDepartureQueue[*uidParent]->vtChildUpdates.push_back(std::make_pair(uidChild, m_mpDepartureQueue[uidChild]->uidUpdated));
			}
			else
			{
				std::shared_ptr<UIDUpdate> ptrUpdateEntryParent = std::make_shared<UIDUpdate>();
				ptrUpdateEntryParent->uidUpdated = std::nullopt;
				ptrUpdateEntryParent->uidParent = std::nullopt;
				ptrUpdateEntryParent->vtChildUpdates.push_back(std::make_pair(uidChild, m_mpDepartureQueue[uidChild]->uidUpdated));

				m_mpDepartureQueue[*uidParent] = ptrUpdateEntryParent;
			}


			m_mpDepartureQueue[uidChild]->uidParent = uidParent;
		}

		return CacheErrorCode::Error;
	}
#endif __POSITION_AWARE_ITEMS__

	void getCacheState(size_t& lru, size_t& map)
	{
		lru = 0;
		std::shared_ptr<Item> _ptrItem = m_ptrHead;
		do
		{
			lru++;
			_ptrItem = _ptrItem->m_ptrNext;

		} while (_ptrItem != nullptr);

		map = m_mpObjects.size();
	}

private:
	void moveToTail(std::shared_ptr<Item> tail, std::shared_ptr<Item> nodeToMove) 
	{
		if (tail == nullptr || nodeToMove == nullptr)
		{
			return;
		}

		if (nodeToMove->m_ptrPrev != nullptr)
		{
			nodeToMove->m_ptrPrev->m_ptrNext = nodeToMove->m_ptrNext;
		}
		else
		{
			tail = nodeToMove->m_ptrNext;
		}

		if (nodeToMove->m_ptrNext != nullptr)
		{
			nodeToMove->m_ptrNext->m_ptrPrev = nodeToMove->m_ptrPrev;
		}

		if (tail != nullptr) 
		{
			tail->m_ptrNext = nodeToMove;
			nodeToMove->m_ptrPrev = tail;
			nodeToMove->m_ptrNext = nullptr;
			tail = nodeToMove;
		}
		else 
		{
			tail = nodeToMove;
		}
	}

	void interchangeWithTail(std::shared_ptr<Item> currentNode) {
		if (currentNode == nullptr || currentNode == m_ptrTail) 
		{
			return;
		}

		if (currentNode->m_ptrPrev) 
		{
			currentNode->m_ptrPrev->m_ptrNext = currentNode->m_ptrNext;
		}
		else 
		{
			m_ptrHead = currentNode->m_ptrNext;
		}

		if (currentNode->m_ptrNext) 
		{
			currentNode->m_ptrNext->m_ptrPrev = currentNode->m_ptrPrev;
		}

		currentNode->m_ptrPrev = m_ptrTail;
		currentNode->m_ptrNext = nullptr;

		m_ptrTail->m_ptrNext = currentNode;

		m_ptrTail = currentNode;
	}

	inline void moveToFront(std::shared_ptr<Item> ptrItem)
	{
		if (ptrItem == m_ptrHead)
		{
			return;
		}

		if (ptrItem->m_ptrPrev) 
		{
			ptrItem->m_ptrPrev->m_ptrNext = ptrItem->m_ptrNext;
		}

		if (ptrItem->m_ptrNext) 
		{
			ptrItem->m_ptrNext->m_ptrPrev = ptrItem->m_ptrPrev;
		}

		if (ptrItem == m_ptrTail) 
		{
			m_ptrTail = ptrItem->m_ptrPrev;
		}

		ptrItem->m_ptrPrev = nullptr;
		ptrItem->m_ptrNext = m_ptrHead;

		if (m_ptrHead) 
		{
			m_ptrHead->m_ptrPrev = ptrItem;
		}
		m_ptrHead = ptrItem;
	}

	inline void removeFromLRU(std::shared_ptr<Item> ptrItem)
	{
		if (ptrItem->m_ptrPrev != nullptr) 
		{
			ptrItem->m_ptrPrev->m_ptrNext = ptrItem->m_ptrNext;
		}
		else 
		{
			m_ptrHead = ptrItem->m_ptrNext;
			if (m_ptrHead != nullptr)
			{
				m_ptrHead->m_ptrPrev = nullptr;
			}
		}

		if (ptrItem->m_ptrNext != nullptr) 
		{
			ptrItem->m_ptrNext->m_ptrPrev = ptrItem->m_ptrPrev;
		}
		else 
		{
			m_ptrTail = ptrItem->m_ptrPrev;
			if (m_ptrTail != nullptr)
			{
				m_ptrTail->m_ptrNext = nullptr;
			}
		}
	}

	int getLRUListCount()
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

	inline void flushItemsToStorage()
	{
#ifdef __CONCURRENT__
		std::vector<ObjectFlushRequest<ObjectType>> vtItems;
			
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);

		if (m_mpObjects.size() <= m_nCacheCapacity)
			return;

		size_t nFlushCount = m_mpObjects.size() - m_nCacheCapacity;
		for (size_t idx = 0; idx < nFlushCount; idx++)
		{
			if (!m_ptrTail->m_ptrObject->mutex.try_lock() || m_ptrTail->m_ptrObject.use_count() > 4)
			{
				// in use.. TODO: proceed with the next one.
				break;
			}
			m_ptrTail->m_ptrObject->mutex.unlock();

			if (m_ptrTail->m_uidParent == std::nullopt)
			{
				break;
			}

			std::shared_ptr<Item> _ptrPrev = m_ptrTail->m_ptrPrev;
			while (_ptrPrev->m_ptrPrev != nullptr)
			{
				if (m_ptrTail->m_uidSelf == _ptrPrev->m_uidParent)
				{
					int pre = getLRUListCount();
					interchangeWithTail(_ptrPrev);
					int post = getLRUListCount();
					assert(pre == post);

					_ptrPrev = m_ptrTail->m_ptrPrev;
					continue;
				}
				_ptrPrev = _ptrPrev->m_ptrPrev;
			}

			if (m_ptrTail->m_ptrObject->dirty)
			{
				ObjectFlushRequest<ObjectType> data(m_ptrTail->m_uidSelf, m_ptrTail->m_uidParent, m_ptrTail->m_ptrObject);
				vtItems.push_back(data);
				//m_ptrStorage->addObject(m_ptrTail->m_uidSelf, m_ptrTail->m_ptrObject, m_ptrTail->m_uidParent);
			}

			// todo look for relation as well!
			std::shared_ptr<Item> ptrTemp = m_ptrTail;

			m_mpObjects.erase(ptrTemp->m_uidSelf);

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
		isEventSignaled = false;

		lock_cache.unlock();

		m_ptrCallback->applyExistingUpdates(vtItems, m_mpDepartureQueue);

		auto it = vtItems.begin();
		int _i = 0;
		while (it != vtItems.end())
		{
			_i++;
			if ((*it).ptrObject.use_count() != 1)
			{
				throw new std::exception("should not occur!");
			}

			if (m_mpDepartureQueue.find((*it).uidDetails.uidObject) != m_mpDepartureQueue.end())
			{
				// was added as parent!
				assert(m_mpDepartureQueue[(*it).uidDetails.uidObject]->uidParent == std::nullopt);
				m_mpDepartureQueue[(*it).uidDetails.uidObject]->uidParent = (*it).uidDetails.uidParent;
			}
			else
			{
				std::shared_ptr<UIDUpdate> ptrUpdateEntry = std::make_shared<UIDUpdate>();
				ptrUpdateEntry->uidUpdated = std::nullopt;
				ptrUpdateEntry->uidParent = (*it).uidDetails.uidParent;
				m_mpDepartureQueue[(*it).uidDetails.uidObject] = ptrUpdateEntry;
			}

			if (m_mpDepartureQueue.find(*(*it).uidDetails.uidParent) != m_mpDepartureQueue.end())
			{
				m_mpDepartureQueue[*(*it).uidDetails.uidParent]->vtChildUpdates.push_back((*it).uidDetails.uidObject);
			}
			else
			{
				std::shared_ptr<UIDUpdate> ptrUpdateEntryParent = std::make_shared<UIDUpdate>();
				ptrUpdateEntryParent->uidUpdated = std::nullopt;
				ptrUpdateEntryParent->uidParent = std::nullopt;
				ptrUpdateEntryParent->vtChildUpdates.push_back((*it).uidDetails.uidObject);

				m_mpDepartureQueue[*(*it).uidDetails.uidParent] = ptrUpdateEntryParent;
			}

			it++;
		}


		size_t nOffset = m_ptrStorage->getWritePos();

		lock_storage.unlock();

		size_t nNewOffset = nOffset;
		m_ptrCallback->prepareFlush(vtItems, nNewOffset, m_ptrStorage->getBlockSize(), m_mpDepartureQueue);

		m_ptrStorage->addObjects(vtItems, nNewOffset);
		//vtItems.clear();
		//m_ptrStorage->addObject(m_ptrTail->m_uidSelf, m_ptrTail->m_ptrObject, m_ptrTail->m_uidParent);
		//m_ptrStorage->addObject((*it).first, (*it).second.second, (*it).second.first);
		//m_ptrStorage->addObjects(vtItems);

		
		std::shared_lock<std::shared_mutex> re_lock_storage(m_mtxStorage);
		cv.wait(re_lock_storage, [] { return isEventSignaled; });
		//re_lock_storage.unlock();
		//std::cout << "cv set... LRUCache.";





		//auto it = vtItems.begin();
		//while (it != vtItems.end())
		//{
		//	//m_ptrStorage->addObject(m_ptrTail->m_uidSelf, m_ptrTail->m_ptrObject, m_ptrTail->m_uidParent);
		//	m_ptrStorage->addObject((*it).first, (*it).second.second, (*it).second.first);

		//	if ((*it).second.second.use_count() != 2)
		//	{
		//		throw new std::exception("should not occur!");
		//	}

		//	it++;
		//}

		//vtItems.clear();
#else
		while (m_mpObjects.size() >= m_nCacheCapacity)
		{
#ifdef __POSITION_AWARE_ITEMS__
			if (m_ptrTail->m_uidParent == std::nullopt)
			{
				return;
			}

			std::shared_ptr<Item> _ptrPrev = m_ptrTail->m_ptrPrev;
			while (_ptrPrev->m_ptrPrev != nullptr)
			{
				if (m_ptrTail->m_uidSelf == _ptrPrev->m_uidParent)
				{
					int pre = getLRUListCount();
					interchangeWithTail(_ptrPrev);
					int post = getLRUListCount();
					assert(pre == post);

					_ptrPrev = m_ptrTail->m_ptrPrev;
					continue;
				}
				_ptrPrev = _ptrPrev->m_ptrPrev;
			}

			if (m_ptrTail->m_ptrObject->dirty) 
			{
				m_ptrStorage->addObject(m_ptrTail->m_uidSelf, m_ptrTail->m_ptrObject, m_ptrTail->m_uidParent);
			}
#else
			m_ptrStorage->addObject(m_ptrTail->m_uidSelf, m_ptrTail->m_ptrObject);
#endif __POSITION_AWARE_ITEMS__
			m_mpObjects.erase(m_ptrTail->m_uidSelf);

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

#ifdef __POSITION_AWARE_ITEMS__
public:
	CacheErrorCode updateChildUID(const std::optional<ObjectUIDType>& uidObject, const ObjectUIDType& uidChildOld, const ObjectUIDType& uidChildNew)
	{
		return CacheErrorCode::Success;
	}

	CacheErrorCode updateChildUID(std::vector<UIDUpdateRequest>& vtUIDUpdates)
	{
		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);

		auto it = vtUIDUpdates.begin();
		while (it != vtUIDUpdates.end())
		{


			if (m_mpDepartureQueue.find((*it).uidObject) != m_mpDepartureQueue.end()) // should not look for entry directly!
			{
				std::shared_ptr<UIDUpdate> ptrUIDUpdate = m_mpDepartureQueue[(*it).uidObject];

				assert(ptrUIDUpdate->uidUpdated == std::nullopt);

				ptrUIDUpdate->uidUpdated = (*it).uidObject_Updated;
				assert(ptrUIDUpdate->uidParent == (*it).uidParent);
			}
			else
			{
				throw new std::exception("should not occur!"); // for the time being!
			}

			if (m_mpDepartureQueue.find(*(*it).uidParent) != m_mpDepartureQueue.end())
			{
				std::shared_ptr<UIDUpdate> ptrUIDUpdate = m_mpDepartureQueue[*(*it).uidParent];

				bool _updated = false;
				auto it_child = ptrUIDUpdate->vtChildUpdates.begin();
				while (it_child != ptrUIDUpdate->vtChildUpdates.end())
				{
					if ((*it_child).first == (*it).uidObject)   // must not be null..
					{
						(*it_child).second = (*it).uidObject_Updated;
						_updated = true;
						break;
					}
					it_child++;
				}
				assert(_updated == true);
				//ptrUIDUpdate->vtChildUpdates.push_back(std::make_pair((*it).uidObject, (*it).uidObject_Updated));
			}
			else
			{
				throw new std::exception("should not occur!"); // for the time being!
			}

			it++;
		}

		isEventSignaled = true;

		m_ptrCallback->updateChildUID(vtUIDUpdates);
		cv.notify_all();

		lock_storage.unlock();

		

		return CacheErrorCode::Success;
	}

	void removeUpdates(std::vector<UIDUpdateRequest>& vtUIDUpdates)
	{
		return;
		//std::unique_lock<std::shared_mutex> _lock(m_mtxCache);
		//auto itt = m_mpDepartureQueue.begin();
		//while (itt != m_mpDepartureQueue.end())
		//{
		//	if (m_mpObjects.find((*itt).first) != m_mpObjects.end())
		//	{
		//		throw new std::exception("should not occur!"); // for the time being!
		//	}
		//	itt++;
		//}
		//_lock.unlock();



		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);

		auto it = vtUIDUpdates.begin();
		while (it != vtUIDUpdates.end())
		{
			if (m_mpDepartureQueue.find((*it).uidObject) != m_mpDepartureQueue.end()) // should not look for entry directly!
			{
				m_mpDepartureQueue.erase((*it).uidObject);
			}
			else
			{
				throw new std::exception("should not occur!"); // for the time being!
			}
			it++;
		}

		vtUIDUpdates.clear();
	}

	void prepareFlush(
		std::vector<ObjectFlushRequest<ObjectType>>& vtObjects,
		size_t& nOffset,
		size_t nBlockSize,
		std::unordered_map<ObjectUIDType, std::shared_ptr<UIDUpdate>>& mpUIDUpdates)
	{}

	void applyExistingUpdates(
		std::vector<ObjectFlushRequest<ObjectType>>& vtObjects,
		std::unordered_map<ObjectUIDType, std::shared_ptr<UIDUpdate>>& mpUIDUpdates)
	{}

#endif __POSITION_AWARE_ITEMS__
};