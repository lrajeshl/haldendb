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
#define __TREE_AWARE_CACHE__

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

	ICallback* m_ptrCallback;

	std::shared_ptr<Item> m_ptrHead;
	std::shared_ptr<Item> m_ptrTail;

	std::unique_ptr<StorageType> m_ptrStorage;

	size_t m_nCacheCapacity;
	std::unordered_map<ObjectUIDType, std::shared_ptr<Item>> m_mpObjects;

	std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>> m_mpUpdatedUIDs;

#ifdef __CONCURRENT__
	bool m_bStop;

	std::thread m_threadCacheFlush;

	std::condition_variable_any cv;


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
		CacheErrorCode errCode = CacheErrorCode::Error;

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex>  lock_cache(m_mtxCache);
#endif __CONCURRENT__

		auto it = m_mpObjects.find(uidObject);
		if (it != m_mpObjects.end()) 
		{
			removeFromLRU((*it).second);
			m_mpObjects.erase((*it).first);
			errCode = CacheErrorCode::Success;
		}

		m_ptrStorage->remove(uidObject);

		return errCode;
	}

	CacheErrorCode getObject(const ObjectUIDType uidObject, ObjectTypePtr & ptrObject, std::optional<ObjectUIDType>& uidUpdated)
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

		ObjectUIDType _uidUpdated = uidObject;
		if (m_mpUpdatedUIDs.find(uidObject) != m_mpUpdatedUIDs.end())
		{

#ifdef __CONCURRENT__
			std::optional< ObjectUIDType >& _condition = m_mpUpdatedUIDs[uidObject].first;
			cv.wait(lock_storage, [&_condition] { return _condition != std::nullopt; });
#endif __CONCURRENT__

			//cv.wait(lock_storage, [] { return m_mpUpdatedUIDs[uidObject].first != std::nullopt; });

			uidUpdated = m_mpUpdatedUIDs[uidObject].first;

			assert(uidUpdated != std::nullopt);

			m_mpUpdatedUIDs.erase(uidObject);	// Applied.
			_uidUpdated = *uidUpdated;
		}

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		std::shared_ptr<ObjectType> _ptrObject = m_ptrStorage->getObject(_uidUpdated);


		if (_ptrObject != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(_uidUpdated, _ptrObject);

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(_uidUpdated) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[_uidUpdated];
				moveToFront(ptrItem);
				ptrObject = ptrItem->m_ptrObject;
				return CacheErrorCode::Success;
			}
#endif __CONCURRENT__

			m_mpObjects[_uidUpdated] = ptrItem;

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

	CacheErrorCode reorder(std::vector<std::pair<ObjectUIDType, ObjectTypePtr>>& vt, bool ensure = true)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache); // std::unique_lock due to LRU's linked-list update! is there any better way?
#endif __CONCURRENT__

		while (vt.size() > 0)
		{
			std::pair<ObjectUIDType, ObjectTypePtr> prNode = vt.back();

			if (m_mpObjects.find(prNode.first) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[prNode.first];
				moveToFront(ptrItem);
			}
			else
			{
				if (ensure) 
				{
					throw new std::exception("should not occur!");
				}
			}

			vt.pop_back();
		}

		return CacheErrorCode::Success;
	}

	template <typename Type>
	CacheErrorCode getObjectOfType(const ObjectUIDType key, Type& ptrObject, std::optional<ObjectUIDType>& uidUpdated)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(key) != m_mpObjects.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObjects[key];

			moveToFront(ptrItem);

//#ifdef __CONCURRENT__
//			lock_cache.unlock();
//#endif __CONCURRENT__
			ptrItem->m_ptrObject->dirty = true; //todo fix it later..

			if (std::holds_alternative<Type>(*ptrItem->m_ptrObject->data))
			{
				ptrObject = std::get<Type>(*ptrItem->m_ptrObject->data);
				return CacheErrorCode::Success;
			}

			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);
		lock_cache.unlock();
#endif __CONCURRENT__

		ObjectUIDType _uidUpdated = key;
		if (m_mpUpdatedUIDs.find(key) != m_mpUpdatedUIDs.end())
		{
#ifdef __CONCURRENT__
			std::optional< ObjectUIDType >& _condition = m_mpUpdatedUIDs[key].first;
			cv.wait(lock_storage, [&_condition] { return _condition != std::nullopt; });
#endif __CONCURRENT__

			uidUpdated = m_mpUpdatedUIDs[key].first;

			assert(uidUpdated != std::nullopt);

			m_mpUpdatedUIDs.erase(key);	// Applied.
			_uidUpdated = *uidUpdated;
		}

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		std::shared_ptr<ObjectType> ptrValue = m_ptrStorage->getObject(_uidUpdated);

		if (ptrValue != nullptr)
		{
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(_uidUpdated, ptrValue);

			ptrValue->dirty = true; //todo fix it later..

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(_uidUpdated) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[_uidUpdated];
				moveToFront(ptrItem);

				if (std::holds_alternative<Type>(*ptrItem->m_ptrObject->data))
				{
					ptrObject = std::get<Type>(*ptrItem->m_ptrObject->data);
					return CacheErrorCode::Success;
				}

				return CacheErrorCode::Error;
			}
#endif __CONCURRENT__

			m_mpObjects[_uidUpdated] = ptrItem;

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
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, const ArgsType... args)
	{
		std::shared_ptr<ObjectType> ptrValue = ObjectType::template createObjectOfType<Type>(args...);

		uidObject = ObjectUIDType::createAddressFromVolatilePointer(reinterpret_cast<uintptr_t>(ptrValue.get()));

		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrValue);

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
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, std::shared_ptr<Type>& ptrCoreObject, const ArgsType... args)
	{
		std::shared_ptr<ObjectType> ptrObject = ObjectType::template createObjectOfType<Type>(ptrCoreObject, args...);

		uidObject = ObjectUIDType::createAddressFromVolatilePointer(reinterpret_cast<uintptr_t>(ptrObject.get()));

		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*uidObject, ptrObject);

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObjects.find(*uidObject) != m_mpObjects.end())
		{
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

		std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>> vtItems;

		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);

		if (m_mpObjects.size() <= m_nCacheCapacity)
			return;

		size_t nFlushCount = m_mpObjects.size() - m_nCacheCapacity;

		if (nFlushCount > 100)
			nFlushCount = 100;

		for (size_t idx = 0; idx < nFlushCount; idx++)
		{
			if (m_ptrTail->m_ptrObject.use_count() > 1)
			{
				// in use.. TODO: proceed with the next one.
				break;
			}

			if (!m_ptrTail->m_ptrObject->mutex.try_lock())
			{
				// in use.. TODO: proceed with the next one.
				break;
			}
			m_ptrTail->m_ptrObject->mutex.unlock();


			std::shared_ptr<Item> ptrTemp = m_ptrTail;

			//TODO fix this!!!!**** if (ptrTemp->m_ptrObject->dirty) // this should not be check here.. as node can be affect by the other nodes in the list
			{
				vtItems.push_back(std::make_pair(ptrTemp->m_uidSelf, std::make_pair(std::nullopt, ptrTemp->m_ptrObject)));
			}

			// can we have back to back updates here!!!!

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

		lock_cache.unlock();

		if (m_mpUpdatedUIDs.size() > 0)
		{
			m_ptrCallback->applyExistingUpdates(vtItems, m_mpUpdatedUIDs);
		}
		
		size_t nOffset = m_ptrStorage->getWritePos();

		lock_storage.unlock();

		size_t nNewOffset = nOffset;

		m_ptrCallback->prepareFlush(vtItems, nNewOffset, m_ptrStorage->getBlockSize());

		auto it = vtItems.begin();
		int _i = 0;
		while (it != vtItems.end())
		{
			if ((*it).second.second == nullptr) // dont proceed.. assumption.. nodes are always in the order.. related leaf nodes and then parent nodes..
			{
				it++;
				continue;
			}

			_i++;
			if ((*it).second.second.use_count() != 1)
			{
				throw new std::exception("should not occur!");
			}

			if (m_mpUpdatedUIDs.find((*it).first) != m_mpUpdatedUIDs.end())
			{
				throw new std::exception("should not occur!");
			}
			else
			{
				m_mpUpdatedUIDs[(*it).first] = std::make_pair(std::nullopt, (*it).second.second);
			}

			it++;
		}

		
		for (int ijk = 0; ijk < vtItems.size(); ijk++)
		{
			if (vtItems[ijk].second.second == nullptr)
			{
				vtItems.erase(vtItems.begin() + ijk);
			}
		}

		m_ptrStorage->addObjects(vtItems, nNewOffset);

		it = vtItems.begin();
		while (it != vtItems.end())
		{
			if (m_mpUpdatedUIDs.find((*it).first) != m_mpUpdatedUIDs.end())
			{
				m_mpUpdatedUIDs[(*it).first] = std::make_pair((*it).second.first, (*it).second.second);
			}
			else
			{
				throw new std::exception("should not occur!");
			}

			it++;
		}


		cv.notify_all();

		/*auto it = vtItems.begin();
		while (it != vtItems.end())
		{
			m_ptrStorage->addObject((*it).first, (*it).second);

			if ((*it).second.use_count() != 2)
			{
				throw new std::exception("should not occur!");
			}

			it++;
		}

		vtItems.clear();*/
#else
		while (m_mpObjects.size() > m_nCacheCapacity)
		{
			if (m_ptrTail->m_ptrObject.use_count() > 1)
			{
				// in use.. TODO: proceed with the next one.
				break;
			}

			if (m_mpUpdatedUIDs.size() > 0)
			{
				m_ptrCallback->applyExistingUpdates(m_ptrTail->m_ptrObject, m_mpUpdatedUIDs);
			}

			if (m_ptrTail->m_ptrObject->dirty)
			{
				ObjectUIDType uidUpdated;
				if (m_ptrStorage->addObject(m_ptrTail->m_uidSelf, m_ptrTail->m_ptrObject, uidUpdated) != CacheErrorCode::Success)
				{
					throw new std::exception("should not occur!");
				}

				if (m_mpUpdatedUIDs.find(m_ptrTail->m_uidSelf) != m_mpUpdatedUIDs.end())
				{
					throw new std::exception("should not occur!");
				}

				m_mpUpdatedUIDs[m_ptrTail->m_uidSelf] = std::make_pair(uidUpdated, m_ptrTail->m_ptrObject);
			}
			else
			{
				int i = 0;
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

#ifdef __TREE_AWARE_CACHE__
public:
	CacheErrorCode updateChildUID(const std::optional<ObjectUIDType>& uidObject, const ObjectUIDType& uidChildOld, const ObjectUIDType& uidChildNew)
	{
		return CacheErrorCode::Success;
	}

	CacheErrorCode updateChildUID(std::vector<std::pair<ObjectUIDType, std::pair<ObjectUIDType, ObjectUIDType>>> vtUpdatedUIDs)
	{
		return CacheErrorCode::Success;
	}

	void applyExistingUpdates(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
		, std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUpdatedUIDs)
	{

	}

	void applyExistingUpdates(std::shared_ptr<ObjectType> ptrObject
		, std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUpdatedUIDs)
	{

	}

	void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects
		, size_t& nOffset, size_t nPointerSize)
	{

	}
#endif __TREE_AWARE_CACHE__
};