#pragma once
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <iterator>
#include <iostream>
#include <cmath>
#include <optional>

#include <iostream>
#include <fstream>


#include "ErrorCodes.h"
#define __POSITION_AWARE_ITEMS__

using namespace std;

template <typename KeyType, typename ValueType, typename ObjectUIDType, uint8_t TYPE_UID>
class IndexNode
{
public:
	static const uint8_t UID = TYPE_UID;
	
	ObjectUIDType m_keyParent;

private:
	typedef IndexNode<KeyType, ValueType, ObjectUIDType, UID> SelfType;
	typedef std::vector<KeyType>::const_iterator KeyTypeIterator;
	typedef std::vector<ObjectUIDType>::const_iterator CacheKeyTypeIterator;

	struct INDEXNODESTRUCT
	{
		std::vector<KeyType> m_vtPivots;
		std::vector<ObjectUIDType> m_vtChildren;
	};

private:
	std::shared_ptr<INDEXNODESTRUCT> m_ptrData;

public:
	~IndexNode()
	{
		// TODO: check for ref count?
		m_ptrData.reset();
	}

	IndexNode()
		: m_ptrData(make_shared<INDEXNODESTRUCT>())
	{	
	}

	IndexNode(std::fstream& is)
		: m_ptrData(make_shared<INDEXNODESTRUCT>())
	{
		//uint8_t _k;
		size_t keyCount, valueCount;
		//is.read(reinterpret_cast<char*>(&_k), sizeof(uint8_t));
		is.read(reinterpret_cast<char*>(&keyCount), sizeof(size_t));
		is.read(reinterpret_cast<char*>(&valueCount), sizeof(size_t));

		m_ptrData->m_vtPivots.resize(keyCount);
		m_ptrData->m_vtChildren.resize(valueCount);

		is.read(reinterpret_cast<char*>(m_ptrData->m_vtPivots.data()), keyCount * sizeof(KeyType));
		is.read(reinterpret_cast<char*>(m_ptrData->m_vtChildren.data()), valueCount * sizeof(ObjectUIDType));
	}

	IndexNode(KeyTypeIterator itBeginPivots, KeyTypeIterator itEndPivots, CacheKeyTypeIterator itBeginChildren, CacheKeyTypeIterator itEndChildren)
		: m_ptrData(make_shared<INDEXNODESTRUCT>())
	{
		m_ptrData->m_vtPivots.assign(itBeginPivots, itEndPivots);
		m_ptrData->m_vtChildren.assign(itBeginChildren, itEndChildren);
	}

	IndexNode(const KeyType& pivotKey, const ObjectUIDType& ptrLHSNode, const ObjectUIDType& ptrRHSNode)
		: m_ptrData(make_shared<INDEXNODESTRUCT>())
	{
		m_ptrData->m_vtPivots.push_back(pivotKey);
		m_ptrData->m_vtChildren.push_back(ptrLHSNode);
		m_ptrData->m_vtChildren.push_back(ptrRHSNode);

	}

	inline void insert(const KeyType& pivotKey, const ObjectUIDType& ptrSibling)
	{
		size_t nChildIdx = m_ptrData->m_vtPivots.size();
		for (int nIdx = 0; nIdx < m_ptrData->m_vtPivots.size(); ++nIdx)
		{
			if (pivotKey < m_ptrData->m_vtPivots[nIdx])
			{
				nChildIdx = nIdx;
				break;
			}
		}

		m_ptrData->m_vtPivots.insert(m_ptrData->m_vtPivots.begin() + nChildIdx, pivotKey);
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.begin() + nChildIdx + 1, ptrSibling);
	}

	template <typename CacheType, typename CacheValueType>
#ifdef __POSITION_AWARE_ITEMS__
	inline ErrorCode rebalanceIndexNode(CacheType ptrCache, CacheValueType ptrChild, const KeyType& key, size_t nDegree, ObjectUIDType& cktChild, std::optional<ObjectUIDType>& cktNodeToDelete, std::optional<ObjectUIDType>& uidparent)
#else
	inline ErrorCode rebalanceIndexNode(CacheType ptrCache, CacheValueType ptrChild, const KeyType& key, size_t nDegree, ObjectUIDType& cktChild, std::optional<ObjectUIDType>& cktNodeToDelete)
#endif __POSITION_AWARE_ITEMS__
	{
		std::optional<ObjectUIDType> _k = std::nullopt;

		CacheValueType ptrLHSNode = nullptr;
		CacheValueType ptrRHSNode = nullptr;

		size_t nChildIdx = getChildNodeIdx(key);

		if (nChildIdx > 0)
		{
#ifdef __POSITION_AWARE_ITEMS__
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx - 1], ptrLHSNode, _k);    //TODO: lock
#else
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx - 1], ptrLHSNode);    //TODO: lock
#endif __POSITION_AWARE_ITEMS__

			if (ptrLHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))	// TODO: macro?
			{
				KeyType key;
#ifdef __POSITION_AWARE_ITEMS__
				ptrChild->moveAnEntityFromLHSSibling<CacheType>(ptrLHSNode, m_ptrData->m_vtPivots[nChildIdx - 1], key, uidparent, ptrCache);
#else
				ptrChild->moveAnEntityFromLHSSibling(ptrLHSNode, m_ptrData->m_vtPivots[nChildIdx - 1], key);
#endif __POSITION_AWARE_ITEMS__

				m_ptrData->m_vtPivots[nChildIdx - 1] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
#ifdef __POSITION_AWARE_ITEMS__
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx + 1], ptrRHSNode, _k);    //TODO: lock
#else
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx + 1], ptrRHSNode);    //TODO: lock
#endif __POSITION_AWARE_ITEMS__

			if (ptrRHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
#ifdef __POSITION_AWARE_ITEMS__
				ptrChild->moveAnEntityFromRHSSibling<CacheType>(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx], key, uidparent, ptrCache);
#else
				ptrChild->moveAnEntityFromRHSSibling(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx], key);
#endif __POSITION_AWARE_ITEMS__

				m_ptrData->m_vtPivots[nChildIdx] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx > 0)
		{
#ifdef __POSITION_AWARE_ITEMS__
			ptrLHSNode->mergeNodes<CacheType>(ptrChild, m_ptrData->m_vtPivots[nChildIdx - 1], uidparent, ptrCache);
#else
			ptrLHSNode->mergeNodes(ptrChild, m_ptrData->m_vtPivots[nChildIdx - 1]);
#endif __POSITION_AWARE_ITEMS__

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx - 1);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx);

			cktNodeToDelete = cktChild;

			return ErrorCode::Success;
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
#ifdef __POSITION_AWARE_ITEMS__
			ptrChild->mergeNodes<CacheType>(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx], uidparent, ptrCache);
#else
			ptrChild->mergeNodes(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx]);
#endif __POSITION_AWARE_ITEMS__

			cktNodeToDelete = m_ptrData->m_vtChildren[nChildIdx + 1];

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}

		throw new exception("should not occur!"); // TODO: critical log entry.
	}

	template <typename CacheType, typename CacheValueType>
	inline ErrorCode rebalanceDataNode(CacheType ptrCache, CacheValueType ptrChild, const KeyType& key, size_t nDegree, ObjectUIDType& cktChild, std::optional<ObjectUIDType>& cktNodeToDelete)
	{
		std::optional<ObjectUIDType> _k = std::nullopt;

		CacheValueType ptrLHSNode = nullptr;
		CacheValueType ptrRHSNode = nullptr;

		size_t nChildIdx = getChildNodeIdx(key);


		if (nChildIdx > 0)
		{
#ifdef __POSITION_AWARE_ITEMS__
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx - 1], ptrLHSNode, _k);    //TODO: lock
#else
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx - 1], ptrLHSNode);    //TODO: lock
#endif __POSITION_AWARE_ITEMS__

			if (ptrLHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromLHSSibling(ptrLHSNode, key);

				m_ptrData->m_vtPivots[nChildIdx - 1] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
#ifdef __POSITION_AWARE_ITEMS__
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx + 1], ptrRHSNode, _k);    //TODO: lock
#else
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx + 1], ptrRHSNode);    //TODO: lock
#endif __POSITION_AWARE_ITEMS__

			if (ptrRHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromRHSSibling(ptrRHSNode, key);

				m_ptrData->m_vtPivots[nChildIdx] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx > 0)
		{
			ptrLHSNode->mergeNode(ptrChild);

			//ptrCache->remove(ptrChildptr);

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx - 1);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx);

			cktNodeToDelete = cktChild;

			return ErrorCode::Success;
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
			ptrChild->mergeNode(ptrRHSNode);

			cktNodeToDelete = m_ptrData->m_vtChildren[nChildIdx + 1];

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}
		
		throw new exception("should not occur!"); // TODO: critical log entry.
	}

	inline size_t getKeysCount() {
		return m_ptrData->m_vtPivots.size();
	}

	inline size_t getChildNodeIdx(const KeyType& key)
	{
		size_t nChildIdx = 0;
		while (nChildIdx < m_ptrData->m_vtPivots.size() && key >= m_ptrData->m_vtPivots[nChildIdx])
		{
			nChildIdx++;
		}

		return nChildIdx;
	}

	inline ObjectUIDType getChildAt(size_t nIdx) {
		return m_ptrData->m_vtChildren[nIdx];
	}

	inline ObjectUIDType getChild(const KeyType& key)
	{
		return m_ptrData->m_vtChildren[getChildNodeIdx(key)];
	}

	inline bool requireSplit(size_t nDegree)
	{
		return m_ptrData->m_vtPivots.size() > nDegree;
	}

	inline bool canTriggerSplit(size_t nDegree)
	{
		return m_ptrData->m_vtPivots.size() + 1 > nDegree;
	}

	inline bool canTriggerMerge(size_t nDegree)
	{
		return m_ptrData->m_vtPivots.size() - 1 <= std::ceil(nDegree / 2.0f);	// TODO: macro!

	}

	inline bool requireMerge(size_t nDegree)
	{
		return m_ptrData->m_vtPivots.size() <= std::ceil(nDegree / 2.0f);
	}

	template <typename Cache>
#ifdef __POSITION_AWARE_ITEMS__
	inline ErrorCode split(Cache ptrCache, std::optional<ObjectUIDType>& ptrSibling, std::optional<ObjectUIDType>& keyparent, KeyType& pivotKey)
#else
	inline ErrorCode split(Cache ptrCache, std::optional<ObjectUIDType>& ptrSibling, KeyType& pivotKey)
#endif __POSITION_AWARE_ITEMS__
	{
		size_t nMid = m_ptrData->m_vtPivots.size() / 2;

#ifdef __POSITION_AWARE_ITEMS__
		std::shared_ptr< SelfType> _ptr = nullptr;
		ptrCache->template createObjectOfType<SelfType>(ptrSibling, keyparent, _ptr,
			m_ptrData->m_vtPivots.begin() + nMid + 1, m_ptrData->m_vtPivots.end(),
			m_ptrData->m_vtChildren.begin() + nMid + 1, m_ptrData->m_vtChildren.end());
		_ptr->updateParent<Cache>(ptrCache, *ptrSibling);
#else
		ptrCache->template createObjectOfType<SelfType>(ptrSibling,
			m_ptrData->m_vtPivots.begin() + nMid + 1, m_ptrData->m_vtPivots.end(),
			m_ptrData->m_vtChildren.begin() + nMid + 1, m_ptrData->m_vtChildren.end());
#endif __POSITION_AWARE_ITEMS__

		if (!ptrSibling)
		{
			return ErrorCode::Error;
		}

		pivotKey = m_ptrData->m_vtPivots[nMid];

		m_ptrData->m_vtPivots.resize(nMid);
		m_ptrData->m_vtChildren.resize(nMid + 1);

		return ErrorCode::Success;
	}

#ifdef __POSITION_AWARE_ITEMS__
	template <typename CacheType>
	inline void moveAnEntityFromLHSSibling(shared_ptr<SelfType> ptrLHSSibling, KeyType& keytoassign, KeyType& pivotKey, std::optional<ObjectUIDType>& uidparent, CacheType ptrCache)
#else
	inline void moveAnEntityFromLHSSibling(shared_ptr<SelfType> ptrLHSSibling, KeyType& keytoassign, KeyType& pivotKey)
#endif __POSITION_AWARE_ITEMS__
	{
		KeyType key = ptrLHSSibling->m_ptrData->m_vtPivots.back();
		ObjectUIDType value = ptrLHSSibling->m_ptrData->m_vtChildren.back();

		ptrLHSSibling->m_ptrData->m_vtPivots.pop_back();
		ptrLHSSibling->m_ptrData->m_vtChildren.pop_back();

		m_ptrData->m_vtPivots.insert(m_ptrData->m_vtPivots.begin(), keytoassign);
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.begin(), value);

		pivotKey = key;

#ifdef __POSITION_AWARE_ITEMS__
		if (uidparent)
		ptrCache->updateParentUID(value, *uidparent);
#endif __POSITION_AWARE_ITEMS__

	}

#ifdef __POSITION_AWARE_ITEMS__
	template <typename CacheType>
	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& keytoassign, KeyType& pivotKey, std::optional<ObjectUIDType>& uidparent, CacheType ptrCache)
#else
	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& keytoassign, KeyType& pivotKey)
#endif __POSITION_AWARE_ITEMS__
	{
		KeyType key = ptrRHSSibling->m_ptrData->m_vtPivots.front();
		ObjectUIDType value = ptrRHSSibling->m_ptrData->m_vtChildren.front();

		ptrRHSSibling->m_ptrData->m_vtPivots.erase(ptrRHSSibling->m_ptrData->m_vtPivots.begin());
		ptrRHSSibling->m_ptrData->m_vtChildren.erase(ptrRHSSibling->m_ptrData->m_vtChildren.begin());

		m_ptrData->m_vtPivots.push_back(keytoassign);
		m_ptrData->m_vtChildren.push_back(value);

		pivotKey = key;// ptrRHSSibling->m_ptrData->m_vtPivots.front();

#ifdef __POSITION_AWARE_ITEMS__
		if (uidparent)
		ptrCache->updateParentUID(value, *uidparent);
#endif __POSITION_AWARE_ITEMS__
	}

#ifdef __POSITION_AWARE_ITEMS__
	template <typename CacheType>
	inline void mergeNodes(shared_ptr<SelfType> ptrSibling, KeyType& pivotKey, std::optional<ObjectUIDType>& uidparent, CacheType ptrCache)
#else
	inline void mergeNodes(shared_ptr<SelfType> ptrSibling, KeyType& pivotKey)
#endif __POSITION_AWARE_ITEMS__
	{
		m_ptrData->m_vtPivots.push_back(pivotKey);
		m_ptrData->m_vtPivots.insert(m_ptrData->m_vtPivots.end(), ptrSibling->m_ptrData->m_vtPivots.begin(), ptrSibling->m_ptrData->m_vtPivots.end());
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.end(), ptrSibling->m_ptrData->m_vtChildren.begin(), ptrSibling->m_ptrData->m_vtChildren.end());

#ifdef __POSITION_AWARE_ITEMS__
		if (uidparent)
		{
			auto it = ptrSibling->m_ptrData->m_vtChildren.begin();
			while (it != ptrSibling->m_ptrData->m_vtChildren.end())
			{
				ptrCache->updateParentUID(*it, *uidparent);
				it++;
			}
		}
#endif __POSITION_AWARE_ITEMS__
	}

public:
	inline const char* getSerializedBytes(uint8_t& uidObjectType, size_t& nDataSize)
	{
		nDataSize = sizeof(*m_ptrData);
		uidObjectType = UID;

		return reinterpret_cast<const char*>(m_ptrData.get());
	}

	inline void writeToStream(std::fstream& os, uint8_t& uidObjectType, size_t& nDataSize)
	{
		//std::fstream os("D:\\filestore.hdb", std::ios::binary | std::ios::in | std::ios::out);

		static_assert(
			std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<ObjectUIDType::NodeUID>::value,
			"Can only deserialize POD types with this function");

		uidObjectType = UID;

		size_t keyCount = m_ptrData->m_vtPivots.size();
		size_t valueCount = m_ptrData->m_vtChildren.size();

		nDataSize = sizeof(uint8_t) + (keyCount * sizeof(KeyType)) + (valueCount * sizeof(ValueType)) + sizeof(size_t) + sizeof(size_t);
		//os.seekp(0);
		os.write(reinterpret_cast<const char*>(&UID), sizeof(uint8_t));
		os.write(reinterpret_cast<const char*>(&keyCount), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(&valueCount), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtPivots.data()), keyCount * sizeof(KeyType));
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtChildren.data()), valueCount * sizeof(ObjectUIDType));
		//os.flush();
		//os.seekg(0);

		//uint8_t _k;
		//size_t keyCount_, valueCount_;

		/*os.read(reinterpret_cast<char*>(&_k), sizeof(uint8_t));
		os.read(reinterpret_cast<char*>(&keyCount_), sizeof(size_t));
		os.read(reinterpret_cast<char*>(&valueCount_), sizeof(size_t));*/

		//std::ifstream file("D:\\abc.hdb", std::ios::binary);
		//std::shared_ptr<SelfType> __ = std::make_shared<SelfType>(os);
	}

	void instantiateSelf(std::byte* bytes)
	{
		//return make_shared<SelfType>(reinterpret_cast<INDEXNODESTRUCT*>(bytes));
	}

	ErrorCode updateChildUID(ObjectUIDType uidOld, ObjectUIDType uidNew)
	{
		auto it = m_ptrData->m_vtChildren.begin();
		while (it != m_ptrData->m_vtChildren.end())
		{
			if (*it == uidOld)
			{
				*it = uidNew;
				return ErrorCode::Success;
			}
			it++;
		}
		return ErrorCode::Error;
	}

	template <typename Cache>
	ErrorCode updateParent(Cache ptrCache, ObjectUIDType parentuid)
	{
		auto it = m_ptrData->m_vtChildren.begin();
		while (it != m_ptrData->m_vtChildren.end())
		{
			ptrCache->updateParentUID(*it, parentuid);
			it++;
		}

		return ErrorCode::Error;
	}
public:

	void printkeys()
	{
		std::cout << "[I](";
		for (size_t nIndex = 0; nIndex < m_ptrData->m_vtPivots.size(); nIndex++)
		{
			std::cout << m_ptrData->m_vtPivots[nIndex];
			if (nIndex < m_ptrData->m_vtPivots.size() - 1)
				std::cout << ",";

		}
		std::cout << ")";
	}

	template <typename CacheType, typename CacheValueType, typename DataNodeType>
	void print(CacheType ptrCache, size_t nLevel)
	{
		std::cout << std::string(nLevel, '.').c_str() << " **LEVEL**:[" << nLevel << "] BEGIN" << std::endl;

		for (size_t nIndex = 0; nIndex < m_ptrData->m_vtChildren.size(); nIndex++)
		{
			if (nIndex < m_ptrData->m_vtPivots.size()) 
			{
				std::cout << std::string(nLevel, '.').c_str() << " ==> pivot: " << m_ptrData->m_vtPivots[nIndex] << std::endl;
			}

			std::cout << std::string(nLevel, '.').c_str() << " ==> child: " << std::endl;

			CacheValueType ptrNode = nullptr;
			ptrCache->getObjectOfType(m_ptrData->m_vtChildren[nIndex], ptrNode);
			if (std::holds_alternative<shared_ptr<SelfType>>(*ptrNode->data))
			{
				shared_ptr<SelfType> ptrIndexNode = std::get<shared_ptr<SelfType>>(*ptrNode->data);
				ptrIndexNode->template print<CacheType, CacheValueType, DataNodeType>(ptrCache, nLevel + 1);
			}
			else if (std::holds_alternative<shared_ptr<DataNodeType>>(*ptrNode->data))
			{
				shared_ptr<DataNodeType> ptrDataNode = std::get<shared_ptr<DataNodeType>>(*ptrNode->data);
				ptrDataNode->print(nLevel + 1);
			}
		}

		std::cout << std::string(nLevel, '.').c_str() << " **LEVEL**:[" << nLevel << "] END" << std::endl;
	}

	void wieHiestDu() {
		printf("ich heisse InternalNode.\n");
	}
};