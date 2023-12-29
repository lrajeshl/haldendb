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
#include <assert.h>

#include "ErrorCodes.h"

#define __POSITION_AWARE_ITEMS__

using namespace std;

template <typename KeyType, typename ValueType, typename ObjectUIDType, uint8_t TYPE_UID>
class IndexNode
{
public:
	static const uint8_t UID = TYPE_UID;
	
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

	IndexNode(const char* szData)
		: m_ptrData(make_shared<INDEXNODESTRUCT>())
	{
		size_t nKeyCount, nValueCount = 0;

		size_t nOffset = sizeof(uint8_t);

		memcpy(&nKeyCount, szData + nOffset, sizeof(size_t));
		nOffset += sizeof(size_t);

		memcpy(&nValueCount, szData + nOffset, sizeof(size_t));
		nOffset += sizeof(size_t);

		m_ptrData->m_vtPivots.resize(nKeyCount);
		m_ptrData->m_vtChildren.resize(nValueCount);

		size_t nKeysSize = nKeyCount * sizeof(KeyType);
		memcpy(m_ptrData->m_vtPivots.data(), szData + nOffset, nKeysSize);
		nOffset += nKeysSize;

		size_t nValuesSize = nValueCount * sizeof(ObjectUIDType::NodeUID);
		memcpy(m_ptrData->m_vtChildren.data(), szData + nOffset, nValuesSize);
	}

	IndexNode(std::fstream& is)
		: m_ptrData(make_shared<INDEXNODESTRUCT>())
	{
		size_t nKeyCount, nValueCount;
		is.read(reinterpret_cast<char*>(&nKeyCount), sizeof(size_t));
		is.read(reinterpret_cast<char*>(&nValueCount), sizeof(size_t));

		m_ptrData->m_vtPivots.resize(nKeyCount);
		m_ptrData->m_vtChildren.resize(nValueCount);

		is.read(reinterpret_cast<char*>(m_ptrData->m_vtPivots.data()), nKeyCount * sizeof(KeyType));
		is.read(reinterpret_cast<char*>(m_ptrData->m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::NodeUID));
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

	inline ErrorCode insert(const KeyType& pivotKey, const ObjectUIDType& uidSibling)
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
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.begin() + nChildIdx + 1, uidSibling);

		return ErrorCode::Success;
	}

	template <typename CacheType, typename ObjectCoreType>
	inline ErrorCode rebalanceIndexNode(CacheType ptrCache, const ObjectUIDType& uidChild, ObjectCoreType ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete)
	{
		ObjectCoreType ptrLHSNode = nullptr;
		ObjectCoreType ptrRHSNode = nullptr;

		size_t nChildIdx = getChildNodeIdx(key);

		if (nChildIdx > 0)
		{
			ptrCache->template getObjectOfType<ObjectCoreType>(m_ptrData->m_vtChildren[nChildIdx - 1], ptrLHSNode);    //TODO: lock

			if (ptrLHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))	// TODO: macro?
			{
				KeyType key;
#ifdef __POSITION_AWARE_ITEMS__
				ptrChild->moveAnEntityFromLHSSibling<CacheType>(ptrLHSNode, m_ptrData->m_vtPivots[nChildIdx - 1], key, uidChild, ptrCache);
#else
				ptrChild->moveAnEntityFromLHSSibling(ptrLHSNode, m_ptrData->m_vtPivots[nChildIdx - 1], key);
#endif __POSITION_AWARE_ITEMS__

				m_ptrData->m_vtPivots[nChildIdx - 1] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
			ptrCache->template getObjectOfType<ObjectCoreType>(m_ptrData->m_vtChildren[nChildIdx + 1], ptrRHSNode);    //TODO: lock

			if (ptrRHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
#ifdef __POSITION_AWARE_ITEMS__
				ptrChild->moveAnEntityFromRHSSibling<CacheType>(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx], key, uidChild, ptrCache);
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
			ptrLHSNode->mergeNodes<CacheType>(ptrChild, m_ptrData->m_vtPivots[nChildIdx - 1], m_ptrData->m_vtChildren[nChildIdx - 1], ptrCache);
#else
			ptrLHSNode->mergeNodes(ptrChild, m_ptrData->m_vtPivots[nChildIdx - 1]);
#endif __POSITION_AWARE_ITEMS__

			uidObjectToDelete = m_ptrData->m_vtChildren[nChildIdx];
			if (uidObjectToDelete != uidChild)
			{
				throw new std::exception("should not occur!");
			}

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx - 1);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx);

			//uidObjectToDelete = uidChild;

			return ErrorCode::Success;
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
#ifdef __POSITION_AWARE_ITEMS__
			ptrChild->mergeNodes<CacheType>(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx], uidChild, ptrCache);
#else
			ptrChild->mergeNodes(ptrRHSNode, m_ptrData->m_vtPivots[nChildIdx]);
#endif __POSITION_AWARE_ITEMS__

			assert(uidChild == m_ptrData->m_vtChildren[nChildIdx]);

			uidObjectToDelete = m_ptrData->m_vtChildren[nChildIdx + 1];

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}

		throw new exception("should not occur!"); // TODO: critical log entry.
	}

	template <typename CacheType, typename ObjectCoreType>
	inline ErrorCode rebalanceDataNode(CacheType ptrCache, const ObjectUIDType& uidChild, ObjectCoreType ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete)
	{
		ObjectCoreType ptrLHSNode = nullptr;
		ObjectCoreType ptrRHSNode = nullptr;

		size_t nChildIdx = getChildNodeIdx(key);

		if (nChildIdx > 0)
		{
			ptrCache->template getObjectOfType<ObjectCoreType>(m_ptrData->m_vtChildren[nChildIdx - 1], ptrLHSNode);    //TODO: lock

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
			ptrCache->template getObjectOfType<ObjectCoreType>(m_ptrData->m_vtChildren[nChildIdx + 1], ptrRHSNode);    //TODO: lock

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

			uidObjectToDelete = m_ptrData->m_vtChildren[nChildIdx];
			if (uidObjectToDelete != uidChild)
			{
				throw new std::exception("should not occur!");
			}

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx - 1);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx);

			//uidObjectToDelete = uidChild;

			return ErrorCode::Success;
		}

		if (nChildIdx < m_ptrData->m_vtPivots.size())
		{
			ptrChild->mergeNode(ptrRHSNode);

			uidObjectToDelete = m_ptrData->m_vtChildren[nChildIdx + 1];

			m_ptrData->m_vtPivots.erase(m_ptrData->m_vtPivots.begin() + nChildIdx);
			m_ptrData->m_vtChildren.erase(m_ptrData->m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}
		
		throw new exception("should not occur!"); // TODO: critical log entry.
	}

	inline size_t getKeysCount() 
	{
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

	inline ObjectUIDType getChildAt(size_t nIdx) 
	{
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
		return m_ptrData->m_vtPivots.size() <= std::ceil(nDegree / 2.0f) + 1;	// TODO: macro!

	}

	inline bool requireMerge(size_t nDegree)
	{
		return m_ptrData->m_vtPivots.size() <= std::ceil(nDegree / 2.0f);
	}

	template <typename Cache>
#ifdef __POSITION_AWARE_ITEMS__
	inline ErrorCode split(Cache ptrCache, std::optional<ObjectUIDType>& uidSibling, KeyType& pivotKeyForParent, const std::optional<ObjectUIDType>& uidParent)
#else
	inline ErrorCode split(Cache ptrCache, std::optional<ObjectUIDType>& uidSibling, KeyType& pivotKeyForParent)
#endif __POSITION_AWARE_ITEMS__
	{
		size_t nMid = m_ptrData->m_vtPivots.size() / 2;

#ifdef __POSITION_AWARE_ITEMS__
		std::shared_ptr< SelfType> ptrSibling = nullptr;
		ptrCache->template createObjectOfType<SelfType>(uidSibling, uidParent, ptrSibling
			/*,
			m_ptrData->m_vtPivots.begin() + nMid + 1, m_ptrData->m_vtPivots.end(),
			m_ptrData->m_vtChildren.begin() + nMid + 1, m_ptrData->m_vtChildren.end()*/);
		// fix.. make it better later.
		// bug.. item were copied first..and the callback from ICallback was updating the item later.. look for other areas as well.

		ptrSibling->m_ptrData->m_vtPivots.assign(m_ptrData->m_vtPivots.begin() + nMid + 1, m_ptrData->m_vtPivots.end());
		ptrSibling->m_ptrData->m_vtChildren.assign(m_ptrData->m_vtChildren.begin() + nMid + 1, m_ptrData->m_vtChildren.end());
		ptrSibling->tryUpdateChilrenParent<Cache>(ptrCache, uidSibling);
#else
		ptrCache->template createObjectOfType<SelfType>(uidSibling,
			m_ptrData->m_vtPivots.begin() + nMid + 1, m_ptrData->m_vtPivots.end(),
			m_ptrData->m_vtChildren.begin() + nMid + 1, m_ptrData->m_vtChildren.end());
#endif __POSITION_AWARE_ITEMS__

		if (!uidSibling)
		{
			return ErrorCode::Error;
		}

		pivotKeyForParent = m_ptrData->m_vtPivots[nMid];

		m_ptrData->m_vtPivots.resize(nMid);
		m_ptrData->m_vtChildren.resize(nMid + 1);

		return ErrorCode::Success;
	}

#ifdef __POSITION_AWARE_ITEMS__
	template <typename CacheType>
	inline void moveAnEntityFromLHSSibling(shared_ptr<SelfType> ptrLHSSibling, KeyType& pivotKeyForEntity, KeyType& pivotKeyForParent, const ObjectUIDType& uidParent, CacheType ptrCache)
#else
	inline void moveAnEntityFromLHSSibling(shared_ptr<SelfType> ptrLHSSibling, KeyType& pivotKeyForEntity, KeyType& pivotKeyForParent)
#endif __POSITION_AWARE_ITEMS__
	{
		KeyType key = ptrLHSSibling->m_ptrData->m_vtPivots.back();
		ObjectUIDType value = ptrLHSSibling->m_ptrData->m_vtChildren.back();

		ptrLHSSibling->m_ptrData->m_vtPivots.pop_back();
		ptrLHSSibling->m_ptrData->m_vtChildren.pop_back();

		if (ptrLHSSibling->m_ptrData->m_vtPivots.size() == 0)
		{
			throw new std::exception("should not occur!");
		}

		m_ptrData->m_vtPivots.insert(m_ptrData->m_vtPivots.begin(), pivotKeyForEntity);
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.begin(), value);

		pivotKeyForParent = key;

#ifdef __POSITION_AWARE_ITEMS__
		ptrCache->tryUpdateParentUID(value, uidParent);
#endif __POSITION_AWARE_ITEMS__

	}

#ifdef __POSITION_AWARE_ITEMS__
	template <typename CacheType>
	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& pivotKeyForEntity, KeyType& pivotKeyForParent, const ObjectUIDType& uidParent, CacheType ptrCache)
#else
	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& pivotKeyForEntity, KeyType& pivotKeyForParent)
#endif __POSITION_AWARE_ITEMS__
	{
		KeyType key = ptrRHSSibling->m_ptrData->m_vtPivots.front();
		ObjectUIDType value = ptrRHSSibling->m_ptrData->m_vtChildren.front();

		ptrRHSSibling->m_ptrData->m_vtPivots.erase(ptrRHSSibling->m_ptrData->m_vtPivots.begin());
		ptrRHSSibling->m_ptrData->m_vtChildren.erase(ptrRHSSibling->m_ptrData->m_vtChildren.begin());

		if (ptrRHSSibling->m_ptrData->m_vtPivots.size() == 0)
		{
			throw new std::exception("should not occur!");

		}

		m_ptrData->m_vtPivots.push_back(pivotKeyForEntity);
		m_ptrData->m_vtChildren.push_back(value);

		pivotKeyForParent = key;// ptrRHSSibling->m_ptrData->m_vtPivots.front();

#ifdef __POSITION_AWARE_ITEMS__
		ptrCache->tryUpdateParentUID(value, uidParent);
#endif __POSITION_AWARE_ITEMS__
	}

#ifdef __POSITION_AWARE_ITEMS__
	template <typename CacheType>
	inline void mergeNodes(shared_ptr<SelfType> ptrSibling, KeyType& pivotKey, const ObjectUIDType& uidParent, CacheType ptrCache)
#else
	inline void mergeNodes(shared_ptr<SelfType> ptrSibling, KeyType& pivotKey)
#endif __POSITION_AWARE_ITEMS__
	{
		m_ptrData->m_vtPivots.push_back(pivotKey);
		m_ptrData->m_vtPivots.insert(m_ptrData->m_vtPivots.end(), ptrSibling->m_ptrData->m_vtPivots.begin(), ptrSibling->m_ptrData->m_vtPivots.end());
		m_ptrData->m_vtChildren.insert(m_ptrData->m_vtChildren.end(), ptrSibling->m_ptrData->m_vtChildren.begin(), ptrSibling->m_ptrData->m_vtChildren.end());

#ifdef __POSITION_AWARE_ITEMS__
		auto it = ptrSibling->m_ptrData->m_vtChildren.begin();
		while (it != ptrSibling->m_ptrData->m_vtChildren.end())
		{
			ptrCache->tryUpdateParentUID(*it, uidParent);
			it++;
		}
#endif __POSITION_AWARE_ITEMS__
	}

public:
	inline void writeToStream(std::fstream& os, uint8_t& uidObjectType, size_t& nDataSize)
	{
		static_assert(
			std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<ObjectUIDType::NodeUID>::value,
			"Can only deserialize POD types with this function");

		uidObjectType = UID;

		size_t nKeyCount = m_ptrData->m_vtPivots.size();
		size_t nValueCount = m_ptrData->m_vtChildren.size();

		nDataSize = sizeof(uint8_t) + (nKeyCount * sizeof(KeyType)) + (nValueCount * sizeof(ObjectUIDType::NodeUID)) + sizeof(size_t) + sizeof(size_t);

		os.write(reinterpret_cast<const char*>(&UID), sizeof(uint8_t));
		os.write(reinterpret_cast<const char*>(&nKeyCount), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(&nValueCount), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtPivots.data()), nKeyCount * sizeof(KeyType));
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::NodeUID));	// fix it!

		// hint
		/*
		if (std::is_trivial<ObjectUIDType>::value && std::is_standard_layout<ObjectUIDType>::value)
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::NodeUID));
		else
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::PODType));	

		*/
	}

	inline void serialize(char*& szBuffer, uint8_t& uidObjectType, size_t& nBufferSize)
	{
		static_assert(
			std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<ObjectUIDType::NodeUID>::value,
			"Can only deserialize POD types with this function");

		uidObjectType = UID;

		size_t nKeyCount = m_ptrData->m_vtPivots.size();
		size_t nValueCount = m_ptrData->m_vtChildren.size();

		nBufferSize = sizeof(uint8_t) + (nKeyCount * sizeof(KeyType)) + (nValueCount * sizeof(ObjectUIDType::NodeUID)) + sizeof(size_t) + sizeof(size_t);

		szBuffer = new char[nBufferSize  + 1];
		memset(szBuffer, '\0', nBufferSize + 1);

		size_t nOffset = 0;
		memcpy(szBuffer, &UID, sizeof(uint8_t));
		nOffset += sizeof(uint8_t);

		memcpy(szBuffer + nOffset, &nKeyCount, sizeof(size_t));
		nOffset += sizeof(size_t);

		memcpy(szBuffer + nOffset, &nValueCount, sizeof(size_t));
		nOffset += sizeof(size_t);

		size_t nKeysSize = nKeyCount * sizeof(KeyType);
		memcpy(szBuffer + nOffset, m_ptrData->m_vtPivots.data(), nKeysSize);
		nOffset += nKeysSize;

		size_t nValuesSize = nValueCount * sizeof(ObjectUIDType::NodeUID);
		memcpy(szBuffer + nOffset, m_ptrData->m_vtChildren.data(), nValuesSize);
		nOffset += nValuesSize;

		assert(nBufferSize == nOffset);

		SelfType* _t = new SelfType(szBuffer);
		for (int i = 0; i < _t->m_ptrData->m_vtPivots.size(); i++) 
		{
			assert(_t->m_ptrData->m_vtPivots[i] == m_ptrData->m_vtPivots[i]);
		}
		for (int i = 0; i < _t->m_ptrData->m_vtChildren.size(); i++)
		{
			assert(_t->m_ptrData->m_vtChildren[i] == m_ptrData->m_vtChildren[i]);
		}
		delete _t;

		// hint
		/*
		if (std::is_trivial<ObjectUIDType>::value && std::is_standard_layout<ObjectUIDType>::value)
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::NodeUID));
		else
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::PODType));

		*/
	}

	inline size_t getSize()
	{
		return 
			sizeof(uint8_t)
			+ sizeof(size_t)
			+ sizeof(size_t)
			+ (m_ptrData->m_vtPivots.size() * sizeof(KeyType))
			+ (m_ptrData->m_vtChildren.size() * sizeof(ObjectUIDType::NodeUID));
	}

	void updateChildUID(const ObjectUIDType& uidOld, const ObjectUIDType& uidNew)
	{
		auto it = m_ptrData->m_vtChildren.begin();
		while (it != m_ptrData->m_vtChildren.end())
		{
			if (*it == uidOld)
			{
				*it = uidNew;
				return;
			}
			it++;
		}
		throw new std::exception("should not occur!");
	}

	template <typename Cache>
	void tryUpdateChilrenParent(Cache ptrCache, const std::optional<ObjectUIDType>& parentuid)
	{
		auto it = m_ptrData->m_vtChildren.begin();
		while (it != m_ptrData->m_vtChildren.end())
		{
			ptrCache->tryUpdateParentUID(*it, parentuid);
			it++;
		}
	}

public:
	template <typename CacheType, typename ObjectType, typename DataNodeType>
#ifdef __POSITION_AWARE_ITEMS__
	void print(std::ofstream& out, CacheType ptrCache, size_t nLevel, string prefix, std::optional<ObjectUIDType> uidParent)
#else //__POSITION_AWARE_ITEMS__
	void print(std::ofstream& out, CacheType ptrCache, size_t nLevel, string prefix)
#endif __POSITION_AWARE_ITEMS__
	{
		int nSpace = 7;

		prefix.append(std::string(nSpace - 1, ' '));
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


			ObjectType ptrNode = nullptr;
#ifdef __POSITION_AWARE_ITEMS__
			std::optional<ObjectUIDType> uidCurrentNodeParent;

			uidCurrentNodeParent = uidParent;
			ptrCache->getObject(m_ptrData->m_vtChildren[nIndex], ptrNode, uidCurrentNodeParent);    //TODO: lock

			if (uidCurrentNodeParent != uidParent)
			{
				throw new std::exception("should not occur!");   // TODO: critical log.
			}

			if (uidCurrentNodeParent)
			{
				out << " P[" << (*uidCurrentNodeParent).toString().c_str() <<"], S[" << m_ptrData->m_vtChildren[nIndex].toString().c_str() << "]";
			}
			else
			{
				out << " [NULL]";
			}
#else
			ptrCache->getObject(m_ptrData->m_vtChildren[nIndex], ptrNode);
#endif __POSITION_AWARE_ITEMS__

			out << std::endl;


			if (std::holds_alternative<shared_ptr<SelfType>>(*ptrNode->data))
			{
				shared_ptr<SelfType> ptrIndexNode = std::get<shared_ptr<SelfType>>(*ptrNode->data);

#ifdef __POSITION_AWARE_ITEMS__
				ptrIndexNode->template print<CacheType, ObjectType, DataNodeType>(out, ptrCache, nLevel + 1, prefix, m_ptrData->m_vtChildren[nIndex]);
#else //__POSITION_AWARE_ITEMS__
				ptrIndexNode->template print<CacheType, ObjectType, DataNodeType>(out, ptrCache, nLevel + 1, prefix);
#endif __POSITION_AWARE_ITEMS__
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