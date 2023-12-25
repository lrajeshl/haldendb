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

public:
	struct INDEXNODESTRUCT
	{
		std::vector<KeyType> m_vtPivots;
		std::vector<ObjectUIDType> m_vtChildren;
	};

public:
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
	inline ErrorCode rebalanceIndexNode(CacheType ptrCache, CacheValueType ptrChild, const KeyType& key, size_t nDegree, ObjectUIDType& cktChild, std::optional<ObjectUIDType>& cktNodeToDelete)
	{
		CacheValueType ptrLHSNode = nullptr;
		CacheValueType ptrRHSNode = nullptr;

		size_t nChildIdx = getChildNodeIdx(key);

		if (nChildIdx > 0)
		{
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx - 1], ptrLHSNode);    //TODO: lock
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
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx + 1], ptrRHSNode);    //TODO: lock
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

			cktNodeToDelete = m_ptrData->m_vtChildren[nChildIdx];
			if (cktNodeToDelete != cktChild)
			{
				throw new std::exception("should not occur!");
			}

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx - 1);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx);

			//cktNodeToDelete = cktChild;

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
	inline ErrorCode rebalanceDataNode(CacheType ptrCache, CacheValueType ptrChild, const KeyType& key, size_t nDegree, ObjectUIDType& cktChild, std::optional<ObjectUIDType>& cktNodeToDelete)
	{
		CacheValueType ptrLHSNode = nullptr;
		CacheValueType ptrRHSNode = nullptr;

		size_t nChildIdx = getChildNodeIdx(key);


		if (nChildIdx > 0)
		{
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx - 1], ptrLHSNode);    //TODO: lock
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
			ptrCache->template getObjectOfType<CacheValueType>(m_ptrData->m_vtChildren[nChildIdx + 1], ptrRHSNode);    //TODO: lock
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
			cktNodeToDelete = m_ptrData->m_vtChildren[nChildIdx];
			if (cktNodeToDelete != cktChild)
			{
				throw new std::exception("should not occur!");
			}

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx - 1);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx);

			//cktNodeToDelete = cktChild;

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
		if (m_ptrData->m_vtPivots.size() == 0)
		{
			std::cout << m_ptrData->m_vtPivots.size() - 1;
		}
		return m_ptrData->m_vtPivots.size() <= std::ceil(nDegree / 2.0f) + 1;	// TODO: macro!

	}

	inline bool requireMerge(size_t nDegree)
	{
		if (m_ptrData->m_vtPivots.size() == 0)
		{
			//std::cout << (m_ptrData->m_vtPivots.size() <= std::ceil(nDegree / 2.0f));
		}
		return m_ptrData->m_vtPivots.size() <= std::ceil(nDegree / 2.0f);
	}

	template <typename Cache>
	inline ErrorCode split(Cache ptrCache, std::optional<ObjectUIDType>& ptrSibling, KeyType& pivotKey)
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
		ObjectUIDType value = ptrLHSSibling->m_ptrData->m_vtChildren.back();

		ptrLHSSibling->m_ptrData->m_vtPivots.pop_back();
		ptrLHSSibling->m_ptrData->m_vtChildren.pop_back();

		if (ptrLHSSibling->m_ptrData->m_vtPivots.size() == 0)
		{
			throw new std::exception("should not occur!");
		}

		m_ptrData->m_vtPivots.insert(m_ptrData->m_vtPivots.begin(), keytoassign);
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.begin(), value);

		pivotKey = key;
	}

	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& keytoassign, KeyType& pivotKey)
	{
		KeyType key = ptrRHSSibling->m_ptrData->m_vtPivots.front();
		ObjectUIDType value = ptrRHSSibling->m_ptrData->m_vtChildren.front();

		ptrRHSSibling->m_ptrData->m_vtPivots.erase(ptrRHSSibling->m_ptrData->m_vtPivots.begin());
		ptrRHSSibling->m_ptrData->m_vtChildren.erase(ptrRHSSibling->m_ptrData->m_vtChildren.begin());

		if (ptrRHSSibling->m_ptrData->m_vtPivots.size() == 0)
		{
			throw new std::exception("should not occur!");

		}
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
	void print(std::ofstream& out, CacheType ptrCache, size_t nLevel, string prefix)
	{
		int nSpace = 7;

		prefix.append(std::string(nSpace- 1, ' '));
		prefix.append("|");
		for (size_t nIndex = 0; nIndex < m_ptrData->m_vtChildren.size(); nIndex++)
		{
			out << " " << prefix << std::endl;
			out << " " << prefix << std::string(nSpace, '-').c_str();// << std::endl;

			if (nIndex < m_ptrData->m_vtPivots.size())
			{
				out << " < (" << m_ptrData->m_vtPivots[nIndex] << ")";// << std::endl;
			}
			else {
				out << " >= (" << m_ptrData->m_vtPivots[nIndex - 1] << ")";// << std::endl;
			}


			out << std::endl;

			CacheValueType ptrNode = nullptr;
			ptrCache->getObject(m_ptrData->m_vtChildren[nIndex], ptrNode);
			if (std::holds_alternative<shared_ptr<SelfType>>(*ptrNode->data))
			{
				shared_ptr<SelfType> ptrIndexNode = std::get<shared_ptr<SelfType>>(*ptrNode->data);
				ptrIndexNode->template print<CacheType, CacheValueType, DataNodeType>(out, ptrCache, nLevel + 1, prefix);
			}
			else if (std::holds_alternative<shared_ptr<DataNodeType>>(*ptrNode->data))
			{
				shared_ptr<DataNodeType> ptrDataNode = std::get<shared_ptr<DataNodeType>>(*ptrNode->data);
				ptrDataNode->print(out, nLevel + 1, prefix);
			}
		}
	}

	void wieHiestDu() {
		printf("ich heisse InternalNode.\n");
	}
};