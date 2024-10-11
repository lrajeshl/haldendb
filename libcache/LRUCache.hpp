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

#include "IFlushCallback.h"
#include "VariadicNthType.h"

#define FLUSH_COUNT 100

using namespace std::chrono_literals;

template <typename ICallback, typename StorageType>
class LRUCache : public ICallback
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

	std::shared_ptr<Item> m_ptrHead;
	std::shared_ptr<Item> m_ptrTail;

	std::unique_ptr<StorageType> m_ptrStorage;

	size_t m_nCacheFootprint;
	size_t m_nCacheCapacity;
	std::unordered_map<ObjectUIDType, std::shared_ptr<Item>> m_mpObjects;
	std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>> m_mpUIDUpdates;

#ifdef __CONCURRENT__
	bool m_bStop;

	std::thread m_threadCacheFlush;

	std::condition_variable_any m_cvUIDUpdates;

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

		m_ptrHead.reset();
		m_ptrTail.reset();;
		m_ptrStorage.reset();

		m_mpObjects.clear();

		std::cout << "Cache size: " << m_nCacheFootprint << " bytes" << std::endl;
	}

	template <typename... StorageArgs>
	LRUCache(size_t nCapacity, StorageArgs... args)
		: m_nCacheCapacity(nCapacity)
		, m_nCacheFootprint(0)
		, m_ptrHead(nullptr)
		, m_ptrTail(nullptr)
	{
		m_ptrStorage = std::make_unique<StorageType>(args...);
		
#ifdef __CONCURRENT__
		m_bStop = false;
		m_threadCacheFlush = std::thread(handlerCacheFlush, this);
#endif __CONCURRENT__
	}

	void updateMemoryFootprint(size_t nMemoryFootprint)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		m_nCacheFootprint += nMemoryFootprint;
	}

	template <typename... InitArgs>
	CacheErrorCode init(ICallback* ptrCallback, InitArgs... args)
	{
//#ifdef __CONCURRENT__
//		m_ptrCallback = ptrCallback;
//		return m_ptrStorage->init(this/*getNthElement<0>(args...)*/);
//#else // ! __CONCURRENT__
//		return m_ptrStorage->init(ptrCallback/*getNthElement<0>(args...)*/);
//#endif __CONCURRENT__

		m_ptrCallback = ptrCallback;

		return m_ptrStorage->init(this/*getNthElement<0>(args...)*/);
	}

	CacheErrorCode remove(const ObjectUIDType& uidObject)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		auto it = m_mpObjects.find(uidObject);
		if (it != m_mpObjects.end()) 
		{
			removeFromLRU((*it).second);
			m_mpObjects.erase(((*it).first));
			
			// TODO:
			// m_ptrStorage->remove(uidObject);
			return CacheErrorCode::Success;
		}

		// TODO:
		// m_ptrStorage->remove(uidObject);

		return CacheErrorCode::KeyDoesNotExist;
	}

	CacheErrorCode getObject(const ObjectUIDType& uidObject, ObjectTypePtr& ptrObject, std::optional<ObjectUIDType>& uidUpdated)
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

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage); // TODO: requesting the same key?
		lock_cache.unlock();
#endif __CONCURRENT__

		ObjectUIDType uidTemp = uidObject;

		if (m_mpUIDUpdates.find(uidObject) != m_mpUIDUpdates.end())
		{
#ifdef __CONCURRENT__
			std::optional< ObjectUIDType >& _condition = m_mpUIDUpdates[uidObject].first;
			m_cvUIDUpdates.wait(lock_storage, [&_condition] { return _condition != std::nullopt; });
#endif __CONCURRENT__

			uidUpdated = m_mpUIDUpdates[uidObject].first;

#ifdef VALIDITY_CHECKS
			assert(uidUpdated != std::nullopt);
#endif VALIDITY_CHECKS

			m_mpUIDUpdates.erase(uidObject);
			uidTemp = *uidUpdated;
		}

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		ptrObject = m_ptrStorage->getObject(uidTemp);

		if (ptrObject != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(uidTemp, ptrObject);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(uidTemp) != m_mpObjects.end())
			{
#ifdef __TRACK_CACHE_FOOTPRINT__
				m_nCacheFootprint -= m_mpObjects[uidTemp]->m_ptrObject->getMemoryFootprint();
				m_nCacheFootprint += ptrObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

				std::shared_ptr<Item> ptrItem = m_mpObjects[uidTemp];
				moveToFront(ptrItem);
				return CacheErrorCode::Success;
			}
#endif __CONCURRENT__

			m_mpObjects[ptrItem->m_uidSelf] = ptrItem;

#ifdef __TRACK_CACHE_FOOTPRINT__
			m_nCacheFootprint += ptrObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

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

#ifndef __CONCURRENT__
			flushItemsToStorage();
#endif __CONCURRENT__

			return CacheErrorCode::Success;
		}

		return CacheErrorCode::Error;
	}

	// This method reorders the recently access objects. 
	// It is necessary to ensure that the objects are flushed in order otherwise a child object (data node) may preceed its parent (internal node).
	CacheErrorCode reorder(std::vector<std::pair<ObjectUIDType, ObjectTypePtr>>& vt, bool bEnsure = true)
	{
		// TODO: Need optimization.
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		while (vt.size() > 0)
		{
			std::pair<ObjectUIDType, ObjectTypePtr> prNode = vt.back();

			if (m_mpObjects.find(prNode.first) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[prNode.first];
				moveToFront(ptrItem);	//TODO: How about passing whole list together and re-arrange the list?
			}
			else
			{
				if (bEnsure)
				{
					throw new std::logic_error("Fatal Error. The expected item is missing in cache.");
				}
			}

			vt.pop_back();
		}

		return CacheErrorCode::Success;
	}

	CacheErrorCode reorderOpt(std::vector<std::pair<ObjectUIDType, ObjectTypePtr>>& vtObjects, bool bEnsure = true)
	{
		size_t _test = vtObjects.size();
		std::vector<std::shared_ptr<Item>> vtItems;

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		while (vtObjects.size() > 0)
		{
			std::pair<ObjectUIDType, ObjectTypePtr> prObject = vtObjects.back();

			if (m_mpObjects.find(prObject.first) != m_mpObjects.end())
			{
				vtItems.emplace_back(m_mpObjects[prObject.first]);
			}

			vtObjects.pop_back();
		}

		if (bEnsure)
		{
			assert(_test == vtItems.size());
		}
		if (vtItems.size() > 1)
			moveToFront(vtItems);
		else
			moveToFront(vtItems[0]);

		return CacheErrorCode::Success;
	}

	template <typename Type>
	CacheErrorCode getObjectOfType(const ObjectUIDType& uidObject, Type& ptrCoreObject, std::optional<ObjectUIDType>& uidUpdated)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(uidObject) != m_mpObjects.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObjects[uidObject];
			moveToFront(ptrItem);

			if (std::holds_alternative<Type>(ptrItem->m_ptrObject->getInnerData()))
			{
				ptrCoreObject = std::get<Type>(ptrItem->m_ptrObject->getInnerData());
				return CacheErrorCode::Success;
			}

			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);
		lock_cache.unlock();
#endif __CONCURRENT__

		const ObjectUIDType* uidTemp = &uidObject;
		if (m_mpUIDUpdates.find(uidObject) != m_mpUIDUpdates.end())
		{
#ifdef __CONCURRENT__
			std::optional< ObjectUIDType >& _condition = m_mpUIDUpdates[uidObject].first;
			m_cvUIDUpdates.wait(lock_storage, [&_condition] { return _condition != std::nullopt; });
#endif __CONCURRENT__

			uidUpdated = m_mpUIDUpdates[uidObject].first;

#ifdef VALIDITY_CHECKS
			assert(uidUpdated != std::nullopt);
#endif VALIDITY_CHECKS

			m_mpUIDUpdates.erase(uidObject);
			uidTemp = *uidUpdated;
		}

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		std::shared_ptr<ObjectType> ptrStorageObject = m_ptrStorage->getObject(*uidTemp);

		if (ptrStorageObject != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidTemp, ptrStorageObject);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(*uidTemp) != m_mpObjects.end())
			{
#ifdef __TRACK_CACHE_FOOTPRINT__
				m_nCacheFootprint -= m_mpObjects[*uidTemp]->m_ptrObject->getMemoryFootprint();
				m_nCacheFootprint += ptrStorageObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

				std::shared_ptr<Item> ptrItem = m_mpObjects[*uidTemp];
				moveToFront(ptrItem);

				if (std::holds_alternative<Type>(ptrItem->m_ptrObject->getInnerData()))
				{
					ptrCoreObject = std::get<Type>(ptrItem->m_ptrObject->getInnerData());
					return CacheErrorCode::Success;
				}

				return CacheErrorCode::Error;
			}
#endif __CONCURRENT__

			m_mpObjects[ptrItem->m_uidSelf] = ptrItem;

#ifdef __TRACK_CACHE_FOOTPRINT__
			m_nCacheFootprint += ptrStorageObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

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

			if (std::holds_alternative<Type>(ptrStorageObject->getInnerData()))
			{
				ptrCoreObject = std::get<Type>(ptrStorageObject->getInnerData());
			}

#ifndef __CONCURRENT__
			flushItemsToStorage();
#endif __CONCURRENT__

			return CacheErrorCode::Error;
		}

		ptrCoreObject = nullptr;
		return CacheErrorCode::Error;
	}

	template <typename Type>
	CacheErrorCode getObjectOfType(const ObjectUIDType& uidObject, Type& ptrCoreObject, ObjectTypePtr& ptrStorageObject, std::optional<ObjectUIDType>& uidUpdated)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(uidObject) != m_mpObjects.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObjects[uidObject];
			moveToFront(ptrItem);

			if (std::holds_alternative<Type>(ptrItem->m_ptrObject->getInnerData()))
			{
				ptrStorageObject = ptrItem->m_ptrObject;
				ptrCoreObject = std::get<Type>(ptrItem->m_ptrObject->getInnerData());
				return CacheErrorCode::Success;
			}

			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);
		lock_cache.unlock();
#endif __CONCURRENT__

		const ObjectUIDType* uidTemp = &uidObject;
		if (m_mpUIDUpdates.find(uidObject) != m_mpUIDUpdates.end())
		{
#ifdef __CONCURRENT__
			std::optional< ObjectUIDType >& _condition = m_mpUIDUpdates[uidObject].first;
			m_cvUIDUpdates.wait(lock_storage, [&_condition] { return _condition != std::nullopt; });
#endif __CONCURRENT__

			uidUpdated = m_mpUIDUpdates[uidObject].first;

#ifdef VALIDITY_CHECKS
			assert(uidUpdated != std::nullopt);
#endif VALIDITY_CHECKS

			m_mpUIDUpdates.erase(uidObject);
			uidTemp = &(*uidUpdated);
		}

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		ptrStorageObject = m_ptrStorage->getObject(*uidTemp);

		if (ptrStorageObject != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidTemp, ptrStorageObject);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(*uidTemp) != m_mpObjects.end())
			{
				// TODO: case where other threads might were accessing the same node and added it to the cache.
				// but need to adjust memory_footprint.
#ifdef __TRACK_CACHE_FOOTPRINT__
				m_nCacheFootprint -= m_mpObjects[*uidTemp]->m_ptrObject->getMemoryFootprint();
				m_nCacheFootprint += ptrStorageObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

				std::shared_ptr<Item> ptrItem = m_mpObjects[*uidTemp];
				moveToFront(ptrItem);

				if (std::holds_alternative<Type>(ptrItem->m_ptrObject->getInnerData()))
				{
					ptrCoreObject = std::get<Type>(ptrItem->m_ptrObject->getInnerData());
					return CacheErrorCode::Success;
				}

				return CacheErrorCode::Error;
			}
#endif __CONCURRENT__

			m_mpObjects[ptrItem->m_uidSelf] = ptrItem;

#ifdef __TRACK_CACHE_FOOTPRINT__
			m_nCacheFootprint += ptrStorageObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

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

			if (std::holds_alternative<Type>(ptrStorageObject->getInnerData()))
			{
				ptrCoreObject = std::get<Type>(ptrStorageObject->getInnerData());
			}

#ifndef __CONCURRENT__
			flushItemsToStorage();
#endif __CONCURRENT__

			return CacheErrorCode::Error;
		}

		ptrCoreObject = nullptr;
		ptrStorageObject = nullptr;
		return CacheErrorCode::Error;
	}

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, const ArgsType... args)
	{
		std::shared_ptr<Type> ptrCoreObject = std::make_shared<Type>(args...);

		std::shared_ptr<ObjectType> ptrStorageObject = std::make_shared<ObjectType>(ptrCoreObject);

		ObjectUIDType uidTemp;
		ObjectUIDType::createAddressFromVolatilePointer(uidTemp, Type::UID, reinterpret_cast<uintptr_t>(ptrStorageObject.get()));

		uidObject = uidTemp;

		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrStorageObject);

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(*uidObject) != m_mpObjects.end())
		{
			throw new std::logic_error(".....");
			std::shared_ptr<Item> ptrItem = m_mpObjects[*uidObject];
			ptrItem->m_ptrObject = ptrStorageObject;
			moveToFront(ptrItem);
		}
		else
		{
			m_mpObjects[ptrItem->m_uidSelf] = ptrItem;

#ifdef __TRACK_CACHE_FOOTPRINT__
			m_nCacheFootprint += ptrStorageObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

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

#ifndef __CONCURRENT__
		flushItemsToStorage();
#endif __CONCURRENT__

		return CacheErrorCode::Success;
	}

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, ObjectTypePtr& ptrStorageObject, const ArgsType... args)
	{
		ptrStorageObject = std::make_shared<ObjectType>(std::make_shared<Type>(args...));

		ObjectUIDType uidTemp;
		ObjectUIDType::createAddressFromVolatilePointer(uidTemp, Type::UID, reinterpret_cast<uintptr_t>(ptrStorageObject.get()));
		
		uidObject = uidTemp;

		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrStorageObject);

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(*uidObject) != m_mpObjects.end())
		{
			throw new std::logic_error(".....");
			std::shared_ptr<Item> ptrItem = m_mpObjects[*uidObject];
			ptrItem->m_ptrObject = ptrStorageObject;
			moveToFront(ptrItem);
		}
		else
		{
			m_mpObjects[ptrItem->m_uidSelf] = ptrItem;

#ifdef __TRACK_CACHE_FOOTPRINT__
			m_nCacheFootprint += ptrStorageObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

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

#ifndef __CONCURRENT__
		flushItemsToStorage();
#endif __CONCURRENT__

		return CacheErrorCode::Success;
	}

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, std::shared_ptr<Type>& ptrCoreObject, const ArgsType... args)
	{
		ptrCoreObject = std::make_shared<Type>(args...);

		std::shared_ptr<ObjectType> ptrStorageObject = std::make_shared<ObjectType>(ptrCoreObject);

		uidObject = ObjectUIDType::createAddressFromVolatilePointer(Type::UID, reinterpret_cast<uintptr_t>(ptrStorageObject.get()));

		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrStorageObject);

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(*uidObject) != m_mpObjects.end())
		{
			throw new std::logic_error(".....");
			std::shared_ptr<Item> ptrItem = m_mpObjects[*uidObject];
			ptrItem->m_ptrObject = ptrStorageObject;
			moveToFront(ptrItem);
		}
		else
		{
			m_mpObjects[&ptrItem->m_uidSelf] = ptrItem;

#ifdef __TRACK_CACHE_FOOTPRINT__
			m_nCacheFootprint += ptrStorageObject->getMemoryFootprint();
#endif __TRACK_CACHE_FOOTPRINT__

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

#ifndef __CONCURRENT__
		flushItemsToStorage();
#endif __CONCURRENT__

		return CacheErrorCode::Success;
	}

	void getCacheState(size_t& nObjectsLinkedList, size_t& nObjectsInMap)
	{
		nObjectsLinkedList = 0;
		std::shared_ptr<Item> ptrItem = m_ptrHead;

		while (ptrItem != nullptr)
		{
			nObjectsLinkedList++;
			ptrItem = ptrItem->m_ptrNext;
		} 

		nObjectsInMap = m_mpObjects.size();
	}

	CacheErrorCode flush()
	{
		flushCacheToStorage();

		return CacheErrorCode::Success;
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

	inline void moveToFront(const std::vector<std::shared_ptr<Item>>& itemList)
	{
		if (itemList.empty())
		{
			return;
		}

		// Step 1: Rearrange the items within the vector
		for (size_t i = 0; i < itemList.size(); i++)
		{
			if (i > 0 && itemList[i - 1] == itemList[i]) {
				//throw new std::logic_error("should not occur!");
				continue;
			}

			auto ptrItem = itemList[i];

			// Detach the item from its current neighbors in the linked list
			if (ptrItem->m_ptrPrev)
			{
				ptrItem->m_ptrPrev->m_ptrNext = ptrItem->m_ptrNext;
			}

			if (ptrItem->m_ptrNext)
			{
				ptrItem->m_ptrNext->m_ptrPrev = ptrItem->m_ptrPrev;
			}

			// If the item is the tail, update the tail pointer
			if (ptrItem == m_ptrTail)
			{
				m_ptrTail = ptrItem->m_ptrPrev;
				//m_ptrTail->m_ptrNext = nullptr;
			}

			if (ptrItem == m_ptrHead)
			{
				m_ptrHead = ptrItem->m_ptrNext;
				m_ptrHead->m_ptrPrev = nullptr;
			}

			// Prepare the item for its new position
			ptrItem->m_ptrPrev = (i > 0) ? itemList[i - 1] : nullptr;  // Previous item in the list
			ptrItem->m_ptrNext = (i < itemList.size() - 1) ? itemList[i + 1] : nullptr;  // Next item in the list
		}

		// Step 2: Link the rearranged items to the front of the linked list

		auto firstItem = itemList.front();
		auto lastItem = itemList.back();

		firstItem->m_ptrPrev = nullptr;
		lastItem->m_ptrNext = m_ptrHead;

		// Connect the last item in the vector to the current head of the linked list
		if (m_ptrHead)
		{
			m_ptrHead->m_ptrPrev = lastItem;
		}

		//lastItem->m_ptrNext = m_ptrHead;
		m_ptrHead = firstItem;  // The first item becomes the new head

		// Step 3: If the list was empty, also set the tail
		if (!m_ptrTail)
		{
			m_ptrTail = lastItem;
		}
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

	inline void flushItemsToStorage()
	{
#ifdef __CONCURRENT__
		std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>> vtObjects;

		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);

		//// Calculate the total size
		//size_t totalSize = sizeof(m_mpObjects) + (m_mpObjects.bucket_count() * sizeof(void*)); // Size of the map structure

		//for (const auto& pair : m_mpObjects) {
		//	totalSize += sizeof(pair.first) + sizeof(pair.second) + sizeof(*pair.second); // Size of each key, pointer, and object
		//}

		//std::cout << "Total size: " << totalSize << " bytes" << std::endl;

		if (m_mpObjects.size() <= m_nCacheCapacity)
			return;

		//std::cout << "Cache size: " << m_nCacheFootprint << " bytes" << std::endl;

		uint16_t nFlushCount = m_mpObjects.size() - m_nCacheCapacity;

		for (uint16_t idx = 0; idx < nFlushCount; idx++)
		{
			if (m_ptrTail->m_ptrObject.use_count() > 1)
			{
				/* Info: 
				 * Should proceed with the preceeding one?
				 * But since each operation reorders the items at the end, therefore, the prceeding items would be in use as well!
				 */
				break; 
			}

			// Check if the object is in use
			if (!m_ptrTail->m_ptrObject->tryLockObject())
			{
				/* Info:
				 * Should proceed with the preceeding one?
				 * But since each operation reorders the items at the end, therefore, the prceeding items would be in use as well!
				 */
				break;
			}
			else
			{
				m_ptrTail->m_ptrObject->unlockObject();
			}

			std::shared_ptr<Item> ptrItemToFlush = m_ptrTail;

			vtObjects.push_back(std::make_pair(ptrItemToFlush->m_uidSelf, std::make_pair(std::nullopt, ptrItemToFlush->m_ptrObject)));

			m_mpObjects.erase(ptrItemToFlush->m_uidSelf);

			m_ptrTail = ptrItemToFlush->m_ptrPrev;

			ptrItemToFlush->m_ptrPrev = nullptr;
			ptrItemToFlush->m_ptrNext = nullptr;

			if (m_ptrTail)
			{
				m_ptrTail->m_ptrNext = nullptr;
			}
			else
			{
				m_ptrHead = nullptr;
			}

			ptrItemToFlush.reset();
		}

		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);

		lock_cache.unlock();

		if (m_mpUIDUpdates.size() > 0)
		{
			m_ptrCallback->applyExistingUpdates(vtObjects, m_mpUIDUpdates);
		}

		// TODO: ensure that no other thread should touch the storage related params..
		size_t nNewOffset = 0;
		m_ptrCallback->prepareFlush(vtObjects, m_ptrStorage->getNextAvailableBlockOffset(), nNewOffset, m_ptrStorage->getBlockSize(), m_ptrStorage->getStorageType());

		//m_ptrCallback->prepareFlush(vtObjects, nPos, m_ptrStorage->getBlockSize(), m_ptrStorage->getMediaType());
		
		for(auto itObject = vtObjects.begin(); itObject != vtObjects.end(); itObject++)
		{
#ifdef VALIDITY_CHECKS
			if ((*itObject).second.second.use_count() != 1)
			{
				throw new std::logic_error("should not occur!");
			}

			if (m_mpUIDUpdates.find((*itObject).first) != m_mpUIDUpdates.end())
			{
				throw new std::logic_error("should not occur!");
			}
#endif // VALIDITY_CHECKS
			m_mpUIDUpdates[(*itObject).first] = std::make_pair(std::nullopt, (*itObject).second.second);
		}

		lock_storage.unlock();
		
		m_ptrStorage->addObjects(vtObjects, nNewOffset);

		std::unique_lock<std::shared_mutex> relock_storage(m_mtxStorage);

		for (auto itObject = vtObjects.begin(); itObject != vtObjects.end(); itObject++)
		{
#ifdef VALIDITY_CHECKS
			if (m_mpUIDUpdates.find((*itObject).first) == m_mpUIDUpdates.end())
			{
				throw new std::logic_error("should not occur!");
			}
#endif VALIDITY_CHECKS

			m_mpUIDUpdates[(*itObject).first].first = (*itObject).second.first;
		}
		relock_storage.unlock();

		m_cvUIDUpdates.notify_all();

		vtObjects.clear();
#else //__CONCURRENT__
		while (m_mpObjects.size() > m_nCacheCapacity)
		{
			if (m_ptrTail->m_ptrObject.use_count() > 1)
			{
				/* Info:
				 * Should proceed with the preceeding one?
				 * But since each operation reorders the items at the end, therefore, the prceeding items would be in use as well!
				 */
				break;
			}

			if (m_mpUIDUpdates.size() > 0)
			{
				m_ptrCallback->applyExistingUpdates(m_ptrTail->m_ptrObject, m_mpUIDUpdates);
			}

			if (m_ptrTail->m_ptrObject->getDirtyFlag())
			{

				ObjectUIDType uidUpdated;
				if (m_ptrStorage->addObject(m_ptrTail->m_uidSelf, m_ptrTail->m_ptrObject, uidUpdated) != CacheErrorCode::Success)
				{
					throw new std::logic_error("should not occur!");
				}

				if (m_mpUIDUpdates.find(m_ptrTail->m_uidSelf) != m_mpUIDUpdates.end())
				{
					throw new std::logic_error("should not occur!");
				}

				m_mpUIDUpdates[m_ptrTail->m_uidSelf] = std::make_pair(uidUpdated, m_ptrTail->m_ptrObject);
			}

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

			ptrTemp.reset();
		}
#endif __CONCURRENT__
	}

	inline void flushCacheToStorage()
	{
#ifdef __CONCURRENT__
		//The current implementation blocks the whole cache, should not be flush allowed at the node level!
		//throw new std::logic_error("implementation missing!");
		return; //fix this
#else __CONCURRENT__

		std::shared_ptr<Item> ptrCurrentTail = m_ptrTail;
		int i = 0;
		std::cout << m_mpObjects.size() << std::endl;

		while (ptrCurrentTail)
		{
			//std::cout << i++ << std::endl;
			if (ptrCurrentTail->m_ptrObject.use_count() > 1)
			{
				// Node in use.. technically this should not occur when flush is called.. anyhow abort the operation.. Also handle thsi case properly later!
				/* Info:
				 * Should proceed with the preceeding one?
				 * But since each operation reorders the items at the end, therefore, the prceeding items would be in use as well!
				 */
				return;
			}

			if (ptrCurrentTail->m_ptrObject->getDirtyFlag())
			{
				if (m_mpUIDUpdates.size() > 0)
				{
					m_ptrCallback->applyExistingUpdates(ptrCurrentTail->m_ptrObject, m_mpUIDUpdates);
				}

				ObjectUIDType uidUpdated;
				if (m_ptrStorage->addObject(ptrCurrentTail->m_uidSelf, ptrCurrentTail->m_ptrObject, uidUpdated) != CacheErrorCode::Success)
				{
					throw new std::logic_error("should not occur!");
				}

				if (m_mpUIDUpdates.find(ptrCurrentTail->m_uidSelf) != m_mpUIDUpdates.end())
				{
					throw new std::logic_error("should not occur!");
				}

				m_mpUIDUpdates[ptrCurrentTail->m_uidSelf] = std::make_pair(uidUpdated, ptrCurrentTail->m_ptrObject);

				m_mpObjects[ptrCurrentTail->m_uidSelf] = ptrCurrentTail;
				m_mpObjects.erase(ptrCurrentTail->m_uidSelf);
			}

			//m_mpObjects.erase(m_ptrTail->m_uidSelf);

			//std::shared_ptr<Item> ptrTemp = m_ptrTail;

			ptrCurrentTail = ptrCurrentTail->m_ptrPrev;

			//if (m_ptrTail)
			//{
			//	m_ptrTail->m_ptrNext = nullptr;
			//}
			//else
			//{
			//	m_ptrHead = nullptr;
			//}
		}
#endif __CONCURRENT__
	}

#ifdef __CONCURRENT__
	static void handlerCacheFlush(SelfType* ptrSelf)
	{
		do
		{
			ptrSelf->flushItemsToStorage();

			std::this_thread::sleep_for(1ms);

		} while (!ptrSelf->m_bStop);
	}
#endif __CONCURRENT__

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
		, size_t nOffset, size_t& nNewOffset, uint16_t nBlockSize, ObjectUIDType::StorageMedia nMediaType)
	{
	}
#endif __TREE_WITH_CACHE__
};
