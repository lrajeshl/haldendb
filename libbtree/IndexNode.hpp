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

using namespace std;

template <typename KeyType, typename ValueType, typename ObjectUIDType, typename DataNodeType, uint8_t TYPE_UID>
class IndexNode
{
public:
	static const uint8_t UID = TYPE_UID;
	
private:
	typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, UID> SelfType;

	typedef std::vector<KeyType>::const_iterator KeyTypeIterator;
	typedef std::vector<ObjectUIDType>::const_iterator CacheKeyTypeIterator;

private:
	std::vector<KeyType> m_vtPivots;
	std::vector<ObjectUIDType> m_vtChildren;

public:
	~IndexNode()
	{
		//m_vtPivots.clear();
		//m_vtChildren.clear();
	}

	IndexNode()
	{	
	}

	IndexNode(const IndexNode& source)
	{
		m_vtPivots.assign(source.m_vtPivots.begin(), source.m_vtPivots.end());
		m_vtChildren.assign(source.m_vtChildren.begin(), source.m_vtChildren.end());
	}

	IndexNode(const char* szData)
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<typename ObjectUIDType::NodeUID>::value)
		{
			uint16_t nKeyCount, nValueCount = 0;

			uint32_t nOffset = sizeof(uint8_t);

			memcpy(&nKeyCount, szData + nOffset, sizeof(uint16_t));
			nOffset += sizeof(uint16_t);

			memcpy(&nValueCount, szData + nOffset, sizeof(uint16_t));
			nOffset += sizeof(uint16_t);

			m_vtPivots.resize(nKeyCount);
			//m_vtChildren.resize(nValueCount);

			uint32_t nKeysSize = nKeyCount * sizeof(KeyType);
			memcpy(m_vtPivots.data(), szData + nOffset, nKeysSize);
			nOffset += nKeysSize;

			for (uint16_t nChild = 0; nChild < nValueCount; nChild++)
			{
				m_vtChildren.push_back(szData + nOffset);
				nOffset += sizeof(typename ObjectUIDType::NodeUID);
			}

			//uint32_t nValuesSize = nValueCount * sizeof(typename ObjectUIDType::NodeUID);
			//memcpy(m_vtChildren.data(), szData + nOffset, nValuesSize);
		}
		else
		{
			static_assert(
				std::is_trivial<KeyType>::value &&
				std::is_standard_layout<KeyType>::value &&
				std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
				std::is_standard_layout<typename ObjectUIDType::NodeUID>::value,
				"Non-POD type is provided. Kindly implement custome de/serializer.");
		}
	}

	IndexNode(std::fstream& is)
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<typename ObjectUIDType::NodeUID>::value)
		{
			uint16_t nKeyCount, nValueCount;
			is.read(reinterpret_cast<char*>(&nKeyCount), sizeof(uint16_t));
			is.read(reinterpret_cast<char*>(&nValueCount), sizeof(uint16_t));

			m_vtPivots.resize(nKeyCount);
			//m_vtChildren.resize(nValueCount);

			is.read(reinterpret_cast<char*>(m_vtPivots.data()), nKeyCount * sizeof(KeyType));
			
			
			for (uint16_t nChild = 0; nChild < nValueCount; nChild++)
			{
				ObjectUIDType uid;
				is.read(reinterpret_cast<char*>(&uid), sizeof(typename ObjectUIDType::NodeUID));
				m_vtChildren.push_back(uid);
				//nOffset += sizeof(typename ObjectUIDType::NodeUID);
			}
			//is.read(reinterpret_cast<char*>(m_vtChildren.data()), nValueCount * sizeof(typename ObjectUIDType::NodeUID));
		}
		else
		{
			static_assert(
				std::is_trivial<KeyType>::value &&
				std::is_standard_layout<KeyType>::value &&
				std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
				std::is_standard_layout<typename ObjectUIDType::NodeUID>::value,
				"Non-POD type is provided. Kindly implement custome de/serializer.");
		}
	}

	IndexNode(KeyTypeIterator itBeginPivots, KeyTypeIterator itEndPivots, CacheKeyTypeIterator itBeginChildren, CacheKeyTypeIterator itEndChildren)
	{
		m_vtPivots.assign(itBeginPivots, itEndPivots);
		m_vtChildren.assign(itBeginChildren, itEndChildren);
	}

	IndexNode(const KeyType& pivotKey, const ObjectUIDType& ptrLHSNode, const ObjectUIDType& ptrRHSNode)
	{
		m_vtPivots.push_back(pivotKey);
		m_vtChildren.push_back(ptrLHSNode);
		m_vtChildren.push_back(ptrRHSNode);
	}

	inline ErrorCode insert(const KeyType& pivotKey, const ObjectUIDType& uidSibling)
	{
		size_t nChildIdx = m_vtPivots.size();
		for (int nIdx = 0; nIdx < m_vtPivots.size(); ++nIdx)
		{
			if (pivotKey < m_vtPivots[nIdx])
			{
				nChildIdx = nIdx;
				break;
			}
		}

		m_vtPivots.insert(m_vtPivots.begin() + nChildIdx, pivotKey);
		m_vtChildren.insert(m_vtChildren.begin() + nChildIdx + 1, uidSibling);

		return ErrorCode::Success;
	}

#ifdef __TREE_WITH_CACHE__
	template <typename CacheType>
	inline ErrorCode rebalanceIndexNode(std::shared_ptr<CacheType>& ptrCache, const ObjectUIDType& uidChild, std::shared_ptr<SelfType>& ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete, std::optional<ObjectUIDType>& uidAffectedNode, CacheType::ObjectTypePtr& ptrAffectedNode)
#else __TREE_WITH_CACHE__
		template <typename CacheType>
	inline ErrorCode rebalanceIndexNode(std::shared_ptr<CacheType>& ptrCache, const ObjectUIDType& uidChild, std::shared_ptr<SelfType>& ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete)
#endif __TREE_WITH_CACHE__
	{
		typedef CacheType::ObjectTypePtr ObjectTypePtr;

		ObjectTypePtr lhs_ = nullptr;
		ObjectTypePtr rhs_ = nullptr;
		std::shared_ptr<SelfType> ptrLHSNode = nullptr;
		std::shared_ptr<SelfType> ptrRHSNode = nullptr;

#ifdef __TREE_WITH_CACHE__
		uidAffectedNode = std::nullopt;
		ptrAffectedNode = nullptr;
#endif __TREE_WITH_CACHE__

		size_t nChildIdx = getChildNodeIdx(key);

		if (nChildIdx > 0)
		{
#ifdef __TREE_WITH_CACHE__
			std::optional<ObjectUIDType> uidUpdated = std::nullopt;
			ptrCache->template getObjectOfType<std::shared_ptr<SelfType>>(m_vtChildren[nChildIdx - 1], ptrLHSNode, lhs_, uidUpdated);    //TODO: lock

			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nChildIdx - 1] = *uidUpdated;
			}

			lhs_->setDirtyFlag(true);

			uidAffectedNode = m_vtChildren[nChildIdx - 1];
			ptrAffectedNode = lhs_;
#else __TREE_WITH_CACHE__
			ptrCache->template getObjectOfType<std::shared_ptr<SelfType>>(m_vtChildren[nChildIdx - 1], ptrLHSNode, lhs_);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(lhs_->getMutex());
#endif __CONCURRENT__

			if (ptrLHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))	// TODO: macro?
			{
				KeyType key;
				ptrChild->moveAnEntityFromLHSSibling(ptrLHSNode, m_vtPivots[nChildIdx - 1], key);

				m_vtPivots[nChildIdx - 1] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx < m_vtPivots.size())
		{
#ifdef __TREE_WITH_CACHE__
			std::optional<ObjectUIDType> uidUpdated = std::nullopt;
			ptrCache->template getObjectOfType<std::shared_ptr<SelfType>>(m_vtChildren[nChildIdx + 1], ptrRHSNode, rhs_, uidUpdated);    //TODO: lock

			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nChildIdx + 1] = *uidUpdated;
			}

			rhs_->setDirtyFlag(true);

			uidAffectedNode = m_vtChildren[nChildIdx + 1];
			ptrAffectedNode = rhs_;
#else __TREE_WITH_CACHE__
			ptrCache->template getObjectOfType<std::shared_ptr<SelfType>>(m_vtChildren[nChildIdx + 1], ptrRHSNode, rhs_);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(rhs_->getMutex());
#endif __CONCURRENT__

			if (ptrRHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromRHSSibling(ptrRHSNode, m_vtPivots[nChildIdx], key);

				m_vtPivots[nChildIdx] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx > 0)
		{
#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(lhs_->getMutex());	//Lock acquired twice!!! merge the respective sections!
#endif __CONCURRENT__

			ptrLHSNode->mergeNodes(ptrChild, m_vtPivots[nChildIdx - 1]);

			uidObjectToDelete = m_vtChildren[nChildIdx];
			if (uidObjectToDelete != uidChild)
			{
				throw new std::logic_error("should not occur!");
			}

			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx - 1);
			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx);

			//uidObjectToDelete = uidChild;

			return ErrorCode::Success;
		}

		if (nChildIdx < m_vtPivots.size())
		{
#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(rhs_->getMutex());
#endif __CONCURRENT__

			ptrChild->mergeNodes(ptrRHSNode, m_vtPivots[nChildIdx]);

			assert(uidChild == m_vtChildren[nChildIdx]);

			uidObjectToDelete = m_vtChildren[nChildIdx + 1];

			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx);
			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}

		throw new logic_error("should not occur!"); // TODO: critical log entry.
	}

#ifdef __TREE_WITH_CACHE__
	template <typename CacheType>
	inline ErrorCode rebalanceDataNode(std::shared_ptr<CacheType>& ptrCache, const ObjectUIDType& uidChild, std::shared_ptr<DataNodeType>& ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete, std::optional<ObjectUIDType>& uidAffectedNode, CacheType::ObjectTypePtr& ptrAffectedNode)
#else __TREE_WITH_CACHE__
	template <typename CacheType>
	inline ErrorCode rebalanceDataNode(std::shared_ptr<CacheType>& ptrCache, const ObjectUIDType& uidChild, std::shared_ptr<DataNodeType>& ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete)
#endif __TREE_WITH_CACHE__
	{
		typedef CacheType::ObjectTypePtr ObjectTypePtr;

		ObjectTypePtr lhs_ = nullptr;
		ObjectTypePtr rhs_ = nullptr;

		std::shared_ptr<DataNodeType> ptrLHSNode = nullptr;
		std::shared_ptr<DataNodeType> ptrRHSNode = nullptr;

#ifdef __TREE_WITH_CACHE__
		uidAffectedNode = std::nullopt;
		ptrAffectedNode = nullptr;
#endif __TREE_WITH_CACHE__

		size_t nChildIdx = getChildNodeIdx(key);

		if (nChildIdx > 0)
		{
#ifdef __TREE_WITH_CACHE__
			std::optional<ObjectUIDType> uidUpdated = std::nullopt;
			ptrCache->template getObjectOfType<std::shared_ptr<DataNodeType>>(m_vtChildren[nChildIdx - 1], ptrLHSNode, lhs_, uidUpdated);    //TODO: lock

			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nChildIdx - 1] = *uidUpdated;
			}

			lhs_->setDirtyFlag(true);

			uidAffectedNode = m_vtChildren[nChildIdx - 1];
			ptrAffectedNode = lhs_;
#else __TREE_WITH_CACHE__
			ptrCache->template getObjectOfType<std::shared_ptr<DataNodeType>>(m_vtChildren[nChildIdx - 1], ptrLHSNode, lhs_);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(lhs_->getMutex());
#endif __CONCURRENT__

			if (ptrLHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromLHSSibling(ptrLHSNode, key);

				m_vtPivots[nChildIdx - 1] = key;

				return ErrorCode::Success;
			}
		}

		if (nChildIdx < m_vtPivots.size())
		{
#ifdef __TREE_WITH_CACHE__
			std::optional<ObjectUIDType> uidUpdated = std::nullopt;
			ptrCache->template getObjectOfType<std::shared_ptr<DataNodeType>>(m_vtChildren[nChildIdx + 1], ptrRHSNode, rhs_, uidUpdated);    //TODO: lock

			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nChildIdx + 1] = *uidUpdated;
			}

			rhs_->setDirtyFlag(true);

			uidAffectedNode = m_vtChildren[nChildIdx + 1];
			ptrAffectedNode = rhs_;
#else __TREE_WITH_CACHE__
			ptrCache->template getObjectOfType<std::shared_ptr<DataNodeType>>(m_vtChildren[nChildIdx + 1], ptrRHSNode, rhs_);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(rhs_->getMutex());
#endif __CONCURRENT__

			if (ptrRHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromRHSSibling(ptrRHSNode, key);

				m_vtPivots[nChildIdx] = key;
				return ErrorCode::Success;
			}
		}

		if (nChildIdx > 0)
		{
#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(lhs_->getMutex());
#endif __CONCURRENT__

			ptrLHSNode->mergeNode(ptrChild);

			uidObjectToDelete = m_vtChildren[nChildIdx];
			if (uidObjectToDelete != uidChild)
			{
				throw new std::logic_error("should not occur!");
			}

			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx - 1);
			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx);

			//uidObjectToDelete = uidChild;

			return ErrorCode::Success;
		}

		if (nChildIdx < m_vtPivots.size())
		{
#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(rhs_->getMutex());
#endif __CONCURRENT__

			ptrChild->mergeNode(ptrRHSNode);

			uidObjectToDelete = m_vtChildren[nChildIdx + 1];

			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx);
			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}
		
		throw new logic_error("should not occur!"); // TODO: critical log entry.
	}

	inline size_t getKeysCount() const
	{
		return m_vtPivots.size();
	}

	inline size_t getChildNodeIdx(const KeyType& key) const
	{
		// std::cout << "[[[[";
		// for(int idx=0; idx< m_vtPivots.size(); idx++)
		// 	std::cout << m_vtPivots[idx] << ",";
		// std::cout << "]]]]" << std::endl;

		size_t nChildIdx = 0;
		while (nChildIdx < m_vtPivots.size() && key >= m_vtPivots[nChildIdx])
		{
			nChildIdx++;
		}

		return nChildIdx;
	}

	inline ObjectUIDType getChildAt(size_t nIdx) const 
	{
		return m_vtChildren[nIdx];
	}

	inline ObjectUIDType getChild(const KeyType& key) const
	{
		return m_vtChildren[getChildNodeIdx(key)];
	}

	inline bool requireSplit(size_t nDegree) const
	{
		return m_vtPivots.size() > nDegree;
	}

	inline bool canTriggerSplit(size_t nDegree) const
	{
		return m_vtPivots.size() + 1 > nDegree;
	}

	inline bool canTriggerMerge(size_t nDegree) const
	{
		return m_vtPivots.size() <= std::ceil(nDegree / 2.0f) + 1;	// TODO: macro!

	}

	inline bool requireMerge(size_t nDegree) const
	{
		return m_vtPivots.size() <= std::ceil(nDegree / 2.0f);
	}

	//template <typename CacheType>
	//inline ErrorCode split(std::shared_ptr<CacheType>& ptrCache, std::optional<ObjectUIDType>& uidSibling, KeyType& pivotKeyForParent)
	template <typename CacheType, typename CacheObjectTypePtr>
	inline ErrorCode split(std::shared_ptr<CacheType> ptrCache, std::optional<ObjectUIDType>& uidSibling, CacheObjectTypePtr& ptrSibling, KeyType& pivotKeyForParent)
	{
		size_t nMid = m_vtPivots.size() / 2;

		ptrCache->template createObjectOfType<SelfType>(uidSibling, ptrSibling,
			m_vtPivots.begin() + nMid + 1, m_vtPivots.end(),
			m_vtChildren.begin() + nMid + 1, m_vtChildren.end());

		if (!uidSibling)
		{
			return ErrorCode::Error;
		}

		pivotKeyForParent = m_vtPivots[nMid];

		m_vtPivots.resize(nMid);
		m_vtChildren.resize(nMid + 1);

		return ErrorCode::Success;
	}

	inline ErrorCode split(std::shared_ptr<SelfType> ptrSibling, KeyType& pivotKeyForParent)
	{
		size_t nMid = m_vtPivots.size() / 2;

		ptrSibling->m_vtPivots.assign(m_vtPivots.begin() + nMid + 1, m_vtPivots.end());
		ptrSibling->m_vtChildren.assign(m_vtChildren.begin() + nMid + 1, m_vtChildren.end());

		pivotKeyForParent = m_vtPivots[nMid];

		m_vtPivots.resize(nMid);
		m_vtChildren.resize(nMid + 1);

		return ErrorCode::Success;
	}

	inline void moveAnEntityFromLHSSibling(shared_ptr<SelfType> ptrLHSSibling, KeyType& pivotKeyForEntity, KeyType& pivotKeyForParent)
	{
		KeyType key = ptrLHSSibling->m_vtPivots.back();
		ObjectUIDType value = ptrLHSSibling->m_vtChildren.back();

		ptrLHSSibling->m_vtPivots.pop_back();
		ptrLHSSibling->m_vtChildren.pop_back();

		if (ptrLHSSibling->m_vtPivots.size() == 0)
		{
			throw new std::logic_error("should not occur!");
		}

		m_vtPivots.insert(m_vtPivots.begin(), pivotKeyForEntity);
		m_vtChildren.insert(m_vtChildren.begin(), value);

		pivotKeyForParent = key;
	}

	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& pivotKeyForEntity, KeyType& pivotKeyForParent)
	{
		KeyType key = ptrRHSSibling->m_vtPivots.front();
		ObjectUIDType value = ptrRHSSibling->m_vtChildren.front();

		ptrRHSSibling->m_vtPivots.erase(ptrRHSSibling->m_vtPivots.begin());
		ptrRHSSibling->m_vtChildren.erase(ptrRHSSibling->m_vtChildren.begin());

		if (ptrRHSSibling->m_vtPivots.size() == 0)
		{
			throw new std::logic_error("should not occur!");
		}

		m_vtPivots.push_back(pivotKeyForEntity);
		m_vtChildren.push_back(value);

		pivotKeyForParent = key;// ptrRHSSibling->m_vtPivots.front();
	}

	inline void mergeNodes(shared_ptr<SelfType> ptrSibling, KeyType& pivotKey)
	{
		m_vtPivots.push_back(pivotKey);
		m_vtPivots.insert(m_vtPivots.end(), ptrSibling->m_vtPivots.begin(), ptrSibling->m_vtPivots.end());
		m_vtChildren.insert(m_vtChildren.end(), ptrSibling->m_vtChildren.begin(), ptrSibling->m_vtChildren.end());
	}

public:
	inline void writeToStream(std::fstream& os, uint8_t& uidObjectType, uint32_t& nDataSize) const
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<typename ObjectUIDType::NodeUID>::value)
		{
			uidObjectType = SelfType::UID;

			uint16_t nKeyCount = m_vtPivots.size();
			uint16_t nValueCount = m_vtChildren.size();

			//nDataSize = sizeof(uint8_t) + (nKeyCount * sizeof(KeyType)) + (nValueCount * sizeof(typename ObjectUIDType::NodeUID)) + sizeof(uint16_t) + sizeof(uint16_t);
			nDataSize = sizeof(uint8_t)					// UID
				+ sizeof(uint16_t)						// Total keys
				+ sizeof(uint16_t)						// Total values
				+ (nKeyCount * sizeof(KeyType))			// Size of all keys
				+ (nValueCount * sizeof(typename ObjectUIDType::NodeUID));	// Size of all values

			os.write(reinterpret_cast<const char*>(&uidObjectType), sizeof(uint8_t));
			os.write(reinterpret_cast<const char*>(&nKeyCount), sizeof(uint16_t));
			os.write(reinterpret_cast<const char*>(&nValueCount), sizeof(uint16_t));
			os.write(reinterpret_cast<const char*>(m_vtPivots.data()), nKeyCount * sizeof(KeyType));
			
			for (auto itChild = m_vtChildren.begin(); itChild != m_vtChildren.end(); itChild++)
			{
				os.write(reinterpret_cast<const char*>(&(*itChild).m_uid), sizeof(typename ObjectUIDType::NodeUID));	// fix it!
			}

			auto it = m_vtChildren.begin();
			while (it != m_vtChildren.end())
			{
				if ((*it).getMediaType() < 3)
				{
					throw new std::logic_error("should not occur!");
				}
				it++;
			}

			// hint
			/*
			if (std::is_trivial<ObjectUIDType>::value && std::is_standard_layout<ObjectUIDType>::value)
			os.write(reinterpret_cast<const char*>(m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::NodeUID));
			else
			os.write(reinterpret_cast<const char*>(m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::PODType));

			*/

		}
		else
		{
			static_assert(
				std::is_trivial<KeyType>::value &&
				std::is_standard_layout<KeyType>::value &&
				std::is_trivial<ValueType>::value &&
				std::is_standard_layout<ValueType>::value,
				"Non-POD type is provided. Kindly implement custome de/serializer.");
		}
	}

	inline void serialize(char*& szBuffer, uint8_t& uidObjectType, uint32_t& nBufferSize) const
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<typename ObjectUIDType::NodeUID>::value)
		{
			uidObjectType = UID;

			uint16_t nKeyCount = m_vtPivots.size();
			uint16_t nValueCount = m_vtChildren.size();

			//nBufferSize = sizeof(uint8_t) + (nKeyCount * sizeof(KeyType)) + (nValueCount * sizeof(typename ObjectUIDType::NodeUID)) + sizeof(size_t) + sizeof(size_t);
			nBufferSize = sizeof(uint8_t)					// UID
				+ sizeof(uint16_t)						// Total keys
				+ sizeof(uint16_t)						// Total values
				+ (nKeyCount * sizeof(KeyType))			// Size of all keys
				+ (nValueCount * sizeof(typename ObjectUIDType::NodeUID));	// Size of all values

			szBuffer = new char[nBufferSize + 1];
			memset(szBuffer, 0, nBufferSize + 1);

			size_t nOffset = 0;
			memcpy(szBuffer, &uidObjectType, sizeof(uint8_t));
			nOffset += sizeof(uint8_t);

			memcpy(szBuffer + nOffset, &nKeyCount, sizeof(uint16_t));
			nOffset += sizeof(uint16_t);

			memcpy(szBuffer + nOffset, &nValueCount, sizeof(uint16_t));
			nOffset += sizeof(uint16_t);

			size_t nKeysSize = nKeyCount * sizeof(KeyType);
			memcpy(szBuffer + nOffset, m_vtPivots.data(), nKeysSize);
			nOffset += nKeysSize;

			for (auto itChild = m_vtChildren.begin(); itChild != m_vtChildren.end(); itChild++)
			{
				memcpy(szBuffer + nOffset, reinterpret_cast<const char*>(&(*itChild).m_uid), sizeof(typename ObjectUIDType::NodeUID));
				nOffset += sizeof(typename ObjectUIDType::NodeUID);
			}

			//size_t nValuesSize = nValueCount * sizeof(typename ObjectUIDType::NodeUID);
			//memcpy(szBuffer + nOffset, m_vtChildren.data(), nValuesSize);
			//nOffset += nValuesSize;

			//assert(nBufferSize == nOffset);

			//SelfType* _t = new SelfType(szBuffer);
			//for (int i = 0; i < _t->m_vtPivots.size(); i++)
			//{
			//	assert(_t->m_vtPivots[i] == m_vtPivots[i]);
			//}
			//for (int i = 0; i < _t->m_vtChildren.size(); i++)
			//{
			//	assert(_t->m_vtChildren[i] == m_vtChildren[i]);
			//}
			//delete _t;

			// hint
			/*
			if (std::is_trivial<ObjectUIDType>::value && std::is_standard_layout<ObjectUIDType>::value)
			os.write(reinterpret_cast<const char*>(m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::NodeUID));
			else
			os.write(reinterpret_cast<const char*>(m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::PODType));

			*/
		}
		else
		{
			static_assert(
				std::is_trivial<KeyType>::value &&
				std::is_standard_layout<KeyType>::value &&
				std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
				std::is_standard_layout<typename ObjectUIDType::NodeUID>::value,
				"Non-POD type is provided. Kindly implement custome de/serializer.");
		}

	}

	inline size_t getSize() const
	{
		return
			sizeof(uint8_t)
			+ sizeof(size_t)
			+ sizeof(size_t)
			+ (m_vtPivots.size() * sizeof(KeyType))
			+ (m_vtChildren.size() * sizeof(typename ObjectUIDType::NodeUID));
	}

	void updateChildUID(const ObjectUIDType& uidOld, const ObjectUIDType& uidNew)
	{
		int idx = 0;
		auto it = m_vtChildren.begin();
		while (it != m_vtChildren.end())
		{
			if (*it == uidOld)
			{
				*it = uidNew;
				return;
			}
			it++;
		}

		throw new std::logic_error("should not occur!");
	}

	inline std::vector<ObjectUIDType>::iterator getChildrenBeginIterator()
	{
		return m_vtChildren.begin();		
	}

	inline std::vector<ObjectUIDType>::iterator getChildrenEndIterator()
	{
		return m_vtChildren.end();
	}

public:
	template <typename CacheType, typename ObjectType>
	void print(std::ofstream& out, std::shared_ptr<CacheType>& ptrCache, size_t nLevel, string prefix)
	{
		int nSpace = 7;

		prefix.append(std::string(nSpace - 1, ' '));
		prefix.append("|");
		for (size_t nIndex = 0; nIndex < m_vtChildren.size(); nIndex++)
		{
			out << " " << prefix << std::endl;
			out << " " << prefix << std::string(nSpace, '-').c_str();// << std::endl;

			if (nIndex < m_vtPivots.size())
			{
				out << " < (" << m_vtPivots[nIndex] << ")";// << std::endl;
			}
			else {
				out << " >= (" << m_vtPivots[nIndex - 1] << ")";// << std::endl;
			}


			ObjectType ptrNode = nullptr;
#ifdef __TREE_WITH_CACHE__
			std::optional<ObjectUIDType> uidUpdated = std::nullopt;
			ptrCache->getObject(m_vtChildren[nIndex], ptrNode, uidUpdated);

			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nIndex] = *uidUpdated;
			}
#else __TREE_WITH_CACHE__
			ptrCache->getObject(m_vtChildren[nIndex], ptrNode);
#endif __TREE_WITH_CACHE__

			out << std::endl;


			if (std::holds_alternative<shared_ptr<SelfType>>(ptrNode->getInnerData()))
			{
				shared_ptr<SelfType> ptrIndexNode = std::get<shared_ptr<SelfType>>(ptrNode->getInnerData());

				ptrIndexNode->template print<CacheType, ObjectType>(out, ptrCache, nLevel + 1, prefix);
			}
			else //if (std::holds_alternative<shared_ptr<DataNodeType>>(ptrNode->getInnerData()))
			{
				shared_ptr<DataNodeType> ptrDataNode = std::get<shared_ptr<DataNodeType>>(ptrNode->getInnerData());
				ptrDataNode->print(out, nLevel + 1, prefix);
			}
		}
	}

	void wieHiestDu() {
		printf("ich heisse InternalNode.\n");
	}
};
