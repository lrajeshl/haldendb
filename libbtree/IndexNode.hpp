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

#include "ErrorCodes.h"

using namespace std;

template <typename KeyType, typename ValueType, typename CacheKeyType, uint8_t TYPE_UID>
class IndexNode
{
public:
	static const uint8_t UID = TYPE_UID;

private:
	typedef IndexNode<KeyType, ValueType, CacheKeyType, UID> SelfType;
	typedef std::vector<KeyType>::const_iterator KeyTypeIterator;
	typedef std::vector<CacheKeyType>::const_iterator CacheKeyTypeIterator;

	struct INDEXNODESTRUCT
	{
		std::vector<KeyType> m_vtPivots;
		std::vector<CacheKeyType> m_vtChildren;
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

	IndexNode(const std::vector<std::byte>& bytes)
	{
		INDEXNODESTRUCT ptrData = reinterpret_cast<INDEXNODESTRUCT>(bytes);
		m_ptrData(ptrData);
	}

	IndexNode(KeyTypeIterator itBeginPivots, KeyTypeIterator itEndPivots, CacheKeyTypeIterator itBeginChildren, CacheKeyTypeIterator itEndChildren)
		: m_ptrData(make_shared<INDEXNODESTRUCT>())
	{
		m_ptrData->m_vtPivots.assign(itBeginPivots, itEndPivots);
		m_ptrData->m_vtChildren.assign(itBeginChildren, itEndChildren);
	}

	IndexNode(const KeyType& pivotKey, const CacheKeyType& ptrLHSNode, const CacheKeyType& ptrRHSNode)
		: m_ptrData(make_shared<INDEXNODESTRUCT>())
	{
		m_ptrData->m_vtPivots.push_back(pivotKey);
		m_ptrData->m_vtChildren.push_back(ptrLHSNode);
		m_ptrData->m_vtChildren.push_back(ptrRHSNode);

	}

	inline void insert(const KeyType& pivotKey, const CacheKeyType& ptrSibling)
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
	inline ErrorCode rebalanceIndexNode(CacheType ptrCache, CacheValueType ptrChild, const KeyType& key, size_t nDegree, CacheKeyType& cktChild, std::optional<CacheKeyType>& cktNodeToDelete)
	{
		CacheValueType ptrLHSNode = nullptr;
		CacheValueType ptrRHSNode = nullptr;

		size_t nChildIdx = getChildNodeIdx(key);

		if (nChildIdx > 0)
		{
			ptrLHSNode = ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx - 1]);    //TODO: lock
			if (ptrLHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))	// TODO: macro?
			{
				KeyType key;
				ptrChild->moveAnEntityFromLHSSibling(ptrLHSNode, m_ptrData->m_vtPivots[nChildIdx - 1], key);

				m_ptrData->m_vtPivots[nChildIdx - 1] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
			ptrRHSNode = ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx + 1]);    //TODO: lock
			if (ptrRHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromRHSSibling(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx], key);

				m_ptrData->m_vtPivots[nChildIdx] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx > 0)
		{
			ptrLHSNode->mergeNodes(ptrChild, m_ptrData->m_vtPivots[nChildIdx - 1]);

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx - 1);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx);

			cktNodeToDelete = cktChild;

			return ErrorCode::Success;
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
			ptrChild->mergeNodes(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx]);

			cktNodeToDelete = m_ptrData->m_vtChildren[nChildIdx + 1];

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}

		throw new exception("should not occur!"); // TODO: critical log entry.
	}

	template <typename CacheType, typename CacheValueType>
	inline ErrorCode rebalanceDataNode(CacheType ptrCache, CacheValueType ptrChild, const KeyType& key, size_t nDegree, CacheKeyType& cktChild, std::optional<CacheKeyType>& cktNodeToDelete)
	{
		CacheValueType ptrLHSNode = nullptr;
		CacheValueType ptrRHSNode = nullptr;

		size_t nChildIdx = getChildNodeIdx(key);


		if (nChildIdx > 0)
		{
			ptrLHSNode = ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx - 1]);    //TODO: lock
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
			ptrRHSNode = ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx + 1]);    //TODO: lock
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

	inline CacheKeyType getChildAt(size_t nIdx) {
		return m_ptrData->m_vtChildren[nIdx];
	}

	inline CacheKeyType getChild(const KeyType& key)
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
	inline ErrorCode split(Cache ptrCache, std::optional<CacheKeyType>& ptrSibling, KeyType& pivotKey)
	{
		size_t nMid = m_ptrData->m_vtPivots.size() / 2;

		ptrSibling = ptrCache->template createObjectOfType<SelfType>(
			m_ptrData->m_vtPivots.begin() + nMid + 1, m_ptrData->m_vtPivots.end(),
			m_ptrData->m_vtChildren.begin() + nMid + 1, m_ptrData->m_vtChildren.end());

		if (!ptrSibling)
		{
			return ErrorCode::Error;
		}

		pivotKey = m_ptrData->m_vtPivots[nMid];

		m_ptrData->m_vtPivots.resize(nMid);
		m_ptrData->m_vtChildren.resize(nMid + 1);

		return ErrorCode::Success;
	}

	inline void moveAnEntityFromLHSSibling(shared_ptr<SelfType> ptrLHSSibling, KeyType& keytoassign, KeyType& pivotKey)
	{
		KeyType key = ptrLHSSibling->m_ptrData->m_vtPivots.back();
		CacheKeyType value = ptrLHSSibling->m_ptrData->m_vtChildren.back();

		ptrLHSSibling->m_ptrData->m_vtPivots.pop_back();
		ptrLHSSibling->m_ptrData->m_vtChildren.pop_back();

		m_ptrData->m_vtPivots.insert(m_ptrData->m_vtPivots.begin(), keytoassign);
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.begin(), value);

		pivotKey = key;
	}

	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& keytoassign, KeyType& pivotKey)
	{
		KeyType key = ptrRHSSibling->m_ptrData->m_vtPivots.front();
		CacheKeyType value = ptrRHSSibling->m_ptrData->m_vtChildren.front();

		ptrRHSSibling->m_ptrData->m_vtPivots.erase(ptrRHSSibling->m_ptrData->m_vtPivots.begin());
		ptrRHSSibling->m_ptrData->m_vtChildren.erase(ptrRHSSibling->m_ptrData->m_vtChildren.begin());

		m_ptrData->m_vtPivots.push_back(keytoassign);
		m_ptrData->m_vtChildren.push_back(value);

		pivotKey = key;// ptrRHSSibling->m_ptrData->m_vtPivots.front();
	}

	inline void mergeNodes(shared_ptr<SelfType> ptrSibling, KeyType& pivotKey)
	{
		m_ptrData->m_vtPivots.push_back(pivotKey);
		m_ptrData->m_vtPivots.insert(m_ptrData->m_vtPivots.end(), ptrSibling->m_ptrData->m_vtPivots.begin(), ptrSibling->m_ptrData->m_vtPivots.end());
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.end(), ptrSibling->m_ptrData->m_vtChildren.begin(), ptrSibling->m_ptrData->m_vtChildren.end());
	}

public:
	std::tuple<uint8_t, const std::byte*, size_t> getSerializedBytes()
	{
		const std::byte* bytes = reinterpret_cast<const std::byte*>(m_ptrData.get());
		return std::tuple<uint8_t, const std::byte*, size_t>(UID, bytes, sizeof(INDEXNODESTRUCT));

	}

	void instantiateSelf(std::byte* bytes)
	{
		//return make_shared<SelfType>(reinterpret_cast<INDEXNODESTRUCT*>(bytes));
	}

public:
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

			CacheValueType ptrNode = ptrCache->getObjectOfType(m_ptrData->m_vtChildren[nIndex]);
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