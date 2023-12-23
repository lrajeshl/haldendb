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

//#define __CONCURRENT__
#define __POSITION_AWARE_ITEMS__

template <typename ICallback, typename StorageType>
class LRUCache : public ICallback
{
	typedef LRUCache<ICallback, StorageType> SelfType;

public:
	typedef StorageType::ObjectUIDType ObjectUIDType;
	typedef StorageType::ObjectType ObjectType;
	typedef std::shared_ptr<ObjectType> ObjectTypePtr;
	typedef StorageType::ObjectCoreTypes ObjectCoreTypes;

private:
	struct Item
	{
	public:
#ifdef __POSITION_AWARE_ITEMS__
		std::optional<ObjectUIDType> m_oKeyParent;
#endif __POSITION_AWARE_ITEMS__

		ObjectUIDType m_oKey;
		ObjectTypePtr m_ptrValue;
		std::shared_ptr<Item> m_ptrPrev;
		std::shared_ptr<Item> m_ptrNext;

#ifdef __POSITION_AWARE_ITEMS__
		Item(ObjectUIDType& key, ObjectTypePtr ptrValue, std::optional<ObjectUIDType>& keyParent)
#else
		Item(ObjectUIDType& key, ObjectTypePtr ptrValue)
#endif __POSITION_AWARE_ITEMS__
			: m_ptrNext(nullptr)
			, m_ptrPrev(nullptr)
		{
			m_oKey = key;
			m_ptrValue = ptrValue;

#ifdef __POSITION_AWARE_ITEMS__
			m_oKeyParent = keyParent;
#endif __POSITION_AWARE_ITEMS__
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

	ICallback* m_ptrCallback;

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
	CacheErrorCode init(ICallback* ptrCallback, InitArgs... args)
	{	
#ifdef __CONCURRENT__
		m_ptrCallback = ptrCallback;
		return m_ptrStorage->init(this/*getNthElement<0>(args...)*/);
#else __CONCURRENT__
		return m_ptrStorage->init(ptrCallback/*getNthElement<0>(args...)*/);
#endif __CONCURRENT__
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

		return CacheErrorCode::KeyDoesNotExist;
	}

#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode getObject(ObjectUIDType key, ObjectTypePtr& ptrObject, std::optional<ObjectUIDType>& keyParent)
#else 
	CacheErrorCode getObject(ObjectUIDType key, ObjectTypePtr& ptrObject)
#endif __POSITION_AWARE_ITEMS__
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache); // std::unique_lock due to LRU's linked-list update! is there any better way?
#endif __CONCURRENT__

		if (m_mpObject.find(key) != m_mpObject.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObject[key];
			moveToFront(ptrItem);
			ptrObject = ptrItem->m_ptrValue;
#ifdef __POSITION_AWARE_ITEMS__
			keyParent = ptrItem->m_oKeyParent;
#endif __POSITION_AWARE_ITEMS__
			return CacheErrorCode::Success;
		}

#ifdef __CONCURRENT__
		lock_cache.unlock();
		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage); // TODO: requesting the same key?
#endif __CONCURRENT__

		std::shared_ptr<ObjectType> ptrValue = m_ptrStorage->getObject(key);

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		if (ptrValue != nullptr)
		{
#ifdef __POSITION_AWARE_ITEMS__
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(key, ptrValue, keyParent);
#else
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(key, ptrValue); 
#endif __POSITION_AWARE_ITEMS__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);
#endif __CONCURRENT__

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
			return CacheErrorCode::Success;
		}
		
		return CacheErrorCode::Error;
	}

#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode getObject_(ObjectUIDType key, ObjectTypePtr& ptrObject, std::optional<ObjectUIDType>& keyParent)
#else 
	CacheErrorCode getObject_(ObjectUIDType key, ObjectTypePtr& ptrObject)
#endif __POSITION_AWARE_ITEMS__
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache); // std::unique_lock due to LRU's linked-list update! is there any better way?
#endif __CONCURRENT__

		if (m_mpObject.find(key) != m_mpObject.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObject[key];
			//moveToFront(ptrItem);
			ptrObject = ptrItem->m_ptrValue;
#ifdef __POSITION_AWARE_ITEMS__
			keyParent = ptrItem->m_oKeyParent;
#endif __POSITION_AWARE_ITEMS__
			return CacheErrorCode::Success;
		}

		return CacheErrorCode::Error;
	}

	template <typename Type>
#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode getObjectOfType(ObjectUIDType key, Type& ptrObject, std::optional<ObjectUIDType>& keyParent)
#else
	CacheErrorCode getObjectOfType(ObjectUIDType key, Type& ptrObject)
#endif __POSITION_AWARE_ITEMS__
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
#ifdef __POSITION_AWARE_ITEMS__
				keyParent = ptrItem->m_oKeyParent;
#endif __POSITION_AWARE_ITEMS__
				return CacheErrorCode::Success;
			}

			return CacheErrorCode::Error;
		}

#ifdef __CONCURRENT__
		lock_cache.unlock();
		std::shared_lock<std::shared_mutex> lock_storage(m_mtxStorage);
#endif __CONCURRENT__

		std::shared_ptr<ObjectType> ptrValue = m_ptrStorage->getObject(key);

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		if (ptrValue != nullptr)
		{
#ifdef __POSITION_AWARE_ITEMS__
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(key, ptrValue, keyParent);
#else
			std::shared_ptr<Item> ptrItem = std::make_shared<Item>(key, ptrValue);
#endif __POSITION_AWARE_ITEMS__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> re_lock_cache(m_mtxCache);
#endif __CONCURRENT__

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

			return CacheErrorCode::Error;
		}

		return CacheErrorCode::Error;
	}

	template<class Type, typename... ArgsType>
#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& key, std::optional<ObjectUIDType> keyParent, ArgsType... args)
#else
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& key, ArgsType... args)
#endif __POSITION_AWARE_ITEMS__
	{
		std::shared_ptr<ObjectType> ptrValue = ObjectType::template createObjectOfType<Type>(args...);

		key = ObjectUIDType::createAddressFromVolatilePointer(reinterpret_cast<uintptr_t>(ptrValue.get()));

#ifdef __POSITION_AWARE_ITEMS__
		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*key, ptrValue, keyParent);
#else
		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*key, ptrValue);
#endif __POSITION_AWARE_ITEMS__

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObject.find(*key) != m_mpObject.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObject[*key];
			ptrItem->m_ptrValue = ptrValue;
			moveToFront(ptrItem);
		}
		else
		{
			m_mpObject[*key] = ptrItem;
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
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& key, std::optional<ObjectUIDType> keyParent, std::shared_ptr<Type>& ptrCoreObject, ArgsType... args)
#else
	CacheErrorCode createObjectOfType(std::optional<ObjectUIDType>& key, std::shared_ptr<ObjectType>& ptrObject, ArgsType... args)
#endif __POSITION_AWARE_ITEMS__
	{
		std::shared_ptr<ObjectType> ptrObject = ObjectType::template createObjectOfType<Type>(ptrCoreObject, args...);

		key = ObjectUIDType::createAddressFromVolatilePointer(reinterpret_cast<uintptr_t>(ptrObject.get()));

#ifdef __POSITION_AWARE_ITEMS__
		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*key, ptrObject, keyParent);
#else
		std::shared_ptr<Item> ptrItem = std::make_shared<Item>(*key, ptrObject);
#endif __POSITION_AWARE_ITEMS__

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_cache(m_mtxCache);
#endif __CONCURRENT__

		if (m_mpObject.find(*key) != m_mpObject.end())
		{
			std::shared_ptr<Item> ptrItem = m_mpObject[*key];
			ptrItem->m_ptrValue = ptrObject;
			moveToFront(ptrItem);
		}
		else
		{
			m_mpObject[*key] = ptrItem;
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

	inline void updateParentUID(ObjectUIDType& child, ObjectUIDType& parent)
	{
		if (m_mpObject.find(child) != m_mpObject.end())
		{
			m_mpObject[child]->m_oKeyParent = parent;
		}
		else
		{
			//throw new std::exception("should not occur!"); node could be on file.

		}
	}

	void* gethead()
	{
		if (m_ptrHead== nullptr)
			return NULL;

		return reinterpret_cast<void*>(m_ptrHead.get());
	}

	void* getnext(void* ptr)
	{
		Item* cur = reinterpret_cast<Item*>(ptr);

		if (cur->m_ptrNext == nullptr)
			return NULL;

		return reinterpret_cast<void*>(cur->m_ptrNext.get());
	}

	ObjectUIDType getuid_(void* ptr)
	{
		return reinterpret_cast<Item*>(ptr)->m_oKey;
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

	void printlru()
	{
		return;
		int cnt = 0;
		std::shared_ptr<Item> _ptrItem = m_ptrHead;
		do
		{
			cnt++;
			_ptrItem = _ptrItem->m_ptrNext;

		} while (_ptrItem != nullptr);
		/*
		if (m_mpObject.size() > 100 && cnt < 99)
		{
		}
		else
		{
			return;
		}*/

		std::cout << "Total count: " << m_mpObject.size() << ", cnt: " << cnt << std::endl;
		std::shared_ptr<Item> ptrItem = m_ptrHead;
		do
		{
			std::cout << "[";
			if (ptrItem->m_ptrPrev != nullptr)
				ptrItem->m_ptrPrev->m_oKey.print();
			else
				std::cout << "<>";

			ptrItem->m_oKey.print();

			if (ptrItem->m_ptrNext != nullptr)
				ptrItem->m_ptrNext->m_oKey.print();
			else
				std::cout << "<>";

			std::cout << "]";
			std::cout << "->";// << std::endl;

			ptrItem = ptrItem->m_ptrNext;

		} while (ptrItem != nullptr);
		std::cout << "." << std::endl;
	}
private:
	inline void moveToFront(std::shared_ptr<Item> ptrItem)
	{
		if (ptrItem == m_ptrHead)
		{
			m_ptrHead->m_ptrPrev = nullptr;
			return;
		}

		if (ptrItem == m_ptrTail)
		{
			m_ptrTail = ptrItem->m_ptrPrev;
			m_ptrTail->m_ptrNext = nullptr;
		}
		else
		{
			ptrItem->m_ptrPrev->m_ptrNext = ptrItem->m_ptrNext;
			ptrItem->m_ptrNext->m_ptrPrev = ptrItem->m_ptrPrev;
		}

		ptrItem->m_ptrPrev = nullptr;
		ptrItem->m_ptrNext = m_ptrHead;
		m_ptrHead->m_ptrPrev = ptrItem;
		m_ptrHead = ptrItem;
	}

	inline void removeFromLRU(std::shared_ptr<Item> ptrItem)
	{
		if (ptrItem == m_ptrHead)
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
		ptrItem->m_ptrNext = nullptr;
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
		printlru();
		while (m_mpObject.size() >= m_nCapacity)
		{
			
			int i = 0;
			std::shared_ptr<Item> _temp = m_ptrTail->m_ptrPrev;
			while (_temp->m_ptrPrev != nullptr)
			{
				i++;
				if (m_ptrTail->m_oKey == _temp->m_oKeyParent)
				{
					//printlru();

					std::cout << "before: " << getlrucount() << std::endl;
					interchangeWithTail(_temp);
					std::cout << "after: " << getlrucount() << std::endl;

					//std::shared_ptr<Item> _tail_prv = m_ptrTail->m_ptrPrev;

					//std::shared_ptr<Item> _trgt_prv = _temp->m_ptrPrev;
					//std::shared_ptr<Item> _trgt_nxt = _temp->m_ptrNext;

					//m_ptrTail->m_ptrPrev = _trgt_prv;
					//m_ptrTail->m_ptrNext = _trgt_nxt;

					//_trgt_prv->m_ptrNext = m_ptrTail;
					//_trgt_nxt->m_ptrPrev = m_ptrTail;

					//m_ptrTail = _temp;
					//m_ptrTail->m_ptrNext = nullptr;
					//m_ptrTail->m_ptrPrev = _tail_prv;
					//_tail_prv->m_ptrNext = m_ptrTail;

					//printlru();
					i = 0;
					_temp = m_ptrTail->m_ptrPrev;
					continue;
					//_temp->m_ptrPrev

					//throw new std::exception("should not occur!");   // TODO: critical log.
				}
				_temp = _temp->m_ptrPrev;
			}

#ifdef __POSITION_AWARE_ITEMS__
			if (m_ptrTail->m_oKeyParent == std::nullopt)
			{
				printlru();
		}
			m_ptrStorage->addObject(m_ptrTail->m_oKey, m_ptrTail->m_ptrValue, m_ptrTail->m_oKeyParent);
		
#else
			m_ptrStorage->addObject(m_ptrTail->m_oKey, m_ptrTail->m_ptrValue);
#endif __POSITION_AWARE_ITEMS__
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
		printlru();
#endif __CONCURRENT__
	}

	void moveToTail(std::shared_ptr<Item> tail, std::shared_ptr<Item> nodeToMove) {
		if (tail == nullptr || nodeToMove == nullptr)
			return; // Invalid input

		// Detach the node from its current position
		if (nodeToMove->m_ptrPrev != nullptr)
			nodeToMove->m_ptrPrev->m_ptrNext = nodeToMove->m_ptrNext;
		else
			tail = nodeToMove->m_ptrNext;

		if (nodeToMove->m_ptrNext != nullptr)
			nodeToMove->m_ptrNext->m_ptrPrev = nodeToMove->m_ptrPrev;

		// Attach the node to the tail
		if (tail != nullptr) {
			tail->m_ptrNext = nodeToMove;
			nodeToMove->m_ptrPrev = tail;
			nodeToMove->m_ptrNext = nullptr;
			tail = nodeToMove;
		}
		else {
			// If the list is empty, set both head and tail to the node
			tail = nodeToMove;
		}
	}

	void interchangeWithTail(std::shared_ptr<Item> currentNode) {
		if (currentNode == nullptr || currentNode == m_ptrTail) {
			// Node is already the tail or is null
			return;
		}

		// Adjust prev and next pointers of adjacent nodes
		if (currentNode->m_ptrPrev) {
			currentNode->m_ptrPrev->m_ptrNext = currentNode->m_ptrNext;
	}
		else {
			// If currentNode is the head, update head
			m_ptrHead = currentNode->m_ptrNext;
		}

		if (currentNode->m_ptrNext) {
			currentNode->m_ptrNext->m_ptrPrev = currentNode->m_ptrPrev;
		}

		// Update prev and next pointers of the currentNode
		currentNode->m_ptrPrev = m_ptrTail;
		currentNode->m_ptrNext = nullptr;

		// Update tail's next pointer to the currentNode
		m_ptrTail->m_ptrNext = currentNode;

		// Update tail to be the currentNode
		m_ptrTail = currentNode;
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


	CacheErrorCode keyUpdate(std::optional<ObjectUIDType>& uidParent, ObjectUIDType uidOld, ObjectUIDType uidNew)
	{
		return CacheErrorCode::Success;
	}

	CacheErrorCode keysUpdate(std::vector<std::pair<ObjectUIDType, std::pair<ObjectUIDType, ObjectUIDType>>> vtUpdatedUIDs)
	{
		return CacheErrorCode::Success;
	}
};