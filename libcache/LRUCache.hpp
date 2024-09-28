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

			//std::cout << std::endl;
			return CacheErrorCode::Success;
		}

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage); // TODO: requesting the same key?
		lock_cache.unlock();
#endif __CONCURRENT__

		ObjectUIDType _uidUpdated = uidObject;
				//std::cout << _uidUpdated.toString().c_str() << " , ";

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
//std::cout << _uidUpdated.toString().c_str() << " ,..... ";

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
//std::cout << std::endl;
			return CacheErrorCode::Success;
		} else {
			//std::cout << "is nulllllll!" << std::endl;
		ptrObject == nullptr;
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
			ptrItem->m_ptrObject->setDirtyFlag( true); //todo fix it later..

			if (std::holds_alternative<Type>(ptrItem->m_ptrObject->getInnerData()))
			{
				ptrObject = std::get<Type>(ptrItem->m_ptrObject->getInnerData());
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

			ptrValue->setDirtyFlag( true); //todo fix it later..

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(_uidUpdated) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[_uidUpdated];
				moveToFront(ptrItem);

				if (std::holds_alternative<Type>(ptrItem->m_ptrObject->getInnerData()))
				{
					ptrObject = std::get<Type>(ptrItem->m_ptrObject->getInnerData());
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

			if (std::holds_alternative<Type>(ptrValue->getInnerData()))
			{
				ptrObject = std::get<Type>(ptrValue->getInnerData());
				return CacheErrorCode::Success;
			}

#ifndef __CONCURRENT__
			flushItemsToStorage();
#endif __CONCURRENT__

			return CacheErrorCode::Error;
		}

		return CacheErrorCode::Error;
	}

	template <typename Type>
	CacheErrorCode getObjectOfType(const ObjectUIDType key, Type& ptrObject, ObjectTypePtr& ptrWrapperType, std::optional<ObjectUIDType>& uidUpdated)
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
			ptrItem->m_ptrObject->setDirtyFlag(true); //todo fix it later..

			if (std::holds_alternative<Type>(ptrItem->m_ptrObject->getInnerData()))
			{
				ptrWrapperType = ptrItem->m_ptrObject;
				ptrObject = std::get<Type>(ptrItem->m_ptrObject->getInnerData());
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

			ptrValue->setDirtyFlag(true); //todo fix it later..

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);

			if (m_mpObjects.find(_uidUpdated) != m_mpObjects.end())
			{
				std::shared_ptr<Item> ptrItem = m_mpObjects[_uidUpdated];
				moveToFront(ptrItem);

				if (std::holds_alternative<Type>(ptrItem->m_ptrObject->getInnerData()))
				{
					ptrWrapperType = ptrItem->m_ptrObject;
					ptrObject = std::get<Type>(ptrItem->m_ptrObject->getInnerData());
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

			if (std::holds_alternative<Type>(ptrValue->getInnerData()))
			{
				ptrWrapperType = ptrValue;
				ptrObject = std::get<Type>(ptrValue->getInnerData());
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
		std::shared_ptr<Type> ptr = std::make_shared<Type>(args...);

		std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(ptr);

		uidObject = ObjectUIDType::createAddressFromVolatilePointer(Type::UID, reinterpret_cast<uintptr_t>(ptrObject.get()));

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

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, ObjectTypePtr& ptrObject, const ArgsType... args)
	{
		//std::shared_ptr<Type> ptr = std::make_shared<Type>(args...);

		//ptrObject = std::make_shared<ObjectType>(ptr);

		ptrObject = std::make_shared<ObjectType>(std::make_shared<Type>(args...));

		uidObject = ObjectUIDType::createAddressFromVolatilePointer(Type::UID, reinterpret_cast<uintptr_t>(ptrObject.get()));

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

	template<class Type, typename... ArgsType>
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& uidObject, std::shared_ptr<Type>& ptrCoreObject, const ArgsType... args)
	{
		ptrCoreObject = std::make_shared<Type>(args...);

		std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(ptrCoreObject);

		uidObject = ObjectUIDType::createAddressFromVolatilePointer(Type::UID, reinterpret_cast<uintptr_t>(ptrObject.get()));

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
//std::cout << m_mpObjects.size() << " , ";
		if (m_mpObjects.size() < m_nCacheCapacity)
			return;

		size_t nFlushCount = m_mpObjects.size() - m_nCacheCapacity;

		if (nFlushCount > FLUSH_COUNT)	//todo: should push all the outstanding orders all together?
			nFlushCount = FLUSH_COUNT;

		for (size_t idx = 0; idx < nFlushCount; idx++)
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

//std::cout << ptrItemToFlush->m_uidSelf.toString().c_str() << " , ";

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
		}

		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);

		lock_cache.unlock();

		if (m_mpUpdatedUIDs.size() > 0)
		{
			m_ptrCallback->applyExistingUpdates(vtObjects, m_mpUpdatedUIDs);
		}

		// Important: Ensure that no other thread should write to the stroage as the nPos is use to generate the addresses.
		size_t nPos = m_ptrStorage->getWritePos();

		m_ptrCallback->prepareFlush(vtObjects, nPos, m_ptrStorage->getBlockSize(), m_ptrStorage->getMediaType());

		auto it = vtObjects.begin();
		while (it != vtObjects.end())
		{
			if ((*it).second.second.use_count() != 1)
			{
				throw new std::logic_error("should not occur!");
			}

			if (m_mpUpdatedUIDs.find((*it).first) != m_mpUpdatedUIDs.end())
			{
				throw new std::logic_error("should not occur!");
			}
			else
			{
				m_mpUpdatedUIDs[(*it).first] = std::make_pair(std::nullopt, (*it).second.second);
			}

			it++;
		}

		lock_storage.unlock();
		
		m_ptrStorage->addObjects(vtObjects, nPos);

		it = vtObjects.begin();
		while (it != vtObjects.end())
		{
			if (m_mpUpdatedUIDs.find((*it).first) != m_mpUpdatedUIDs.end())
			{
				m_mpUpdatedUIDs[(*it).first] = std::make_pair((*it).second.first, (*it).second.second);
			}
			else
			{
				throw new std::logic_error("should not occur!");
			}

			it++;
		}

		cv.notify_all();

		vtObjects.clear();
#else
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

			if (m_ptrTail->m_ptrObject->getDirtyFlag())
			{
				if (m_mpUpdatedUIDs.size() > 0)
				{
					m_ptrCallback->applyExistingUpdates(m_ptrTail->m_ptrObject, m_mpUpdatedUIDs);
				}

				ObjectUIDType uidUpdated;
				if (m_ptrStorage->addObject(m_ptrTail->m_uidSelf, m_ptrTail->m_ptrObject, uidUpdated) != CacheErrorCode::Success)
				{
					throw new std::logic_error("should not occur!");
				}

				if (m_mpUpdatedUIDs.find(m_ptrTail->m_uidSelf) != m_mpUpdatedUIDs.end())
				{
					throw new std::logic_error("should not occur!");
				}

				m_mpUpdatedUIDs[m_ptrTail->m_uidSelf] = std::make_pair(uidUpdated, m_ptrTail->m_ptrObject);
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
				if (m_mpUpdatedUIDs.size() > 0)
				{
					m_ptrCallback->applyExistingUpdates(ptrCurrentTail->m_ptrObject, m_mpUpdatedUIDs);
				}

				ObjectUIDType uidUpdated;
				if (m_ptrStorage->addObject(ptrCurrentTail->m_uidSelf, ptrCurrentTail->m_ptrObject, uidUpdated) != CacheErrorCode::Success)
				{
					throw new std::logic_error("should not occur!");
				}

				if (m_mpUpdatedUIDs.find(ptrCurrentTail->m_uidSelf) != m_mpUpdatedUIDs.end())
				{
					throw new std::logic_error("should not occur!");
				}

				m_mpUpdatedUIDs[ptrCurrentTail->m_uidSelf] = std::make_pair(uidUpdated, ptrCurrentTail->m_ptrObject);

				m_mpObjects[uidUpdated] = ptrCurrentTail;
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

			std::this_thread::sleep_for(100ms);

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

	void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects
		, size_t& nOffset, size_t nPointerSize, ObjectUIDType::Media nMediaType)
	{

	}
#endif __TREE_WITH_CACHE__
};
