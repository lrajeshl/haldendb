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

#ifdef _MSC_VER
#define PACKED_STRUCT __pragma(pack(push, 1))
#define END_PACKED_STRUCT __pragma(pack(pop))
#else
#define PACKED_STRUCT _Pragma("pack(push, 1)")
#define END_PACKED_STRUCT _Pragma("pack(pop)")
#endif

using namespace std;

template <typename KeyType, typename ValueType, typename ObjectUIDType, typename DataNodeType, uint8_t TYPE_UID>
class IndexNodeROpt
{
private:

PACKED_STRUCT
	struct RAWDATA
	{
		uint8_t nUID;
		uint16_t nTotalPivots;
		const KeyType* ptrPivots;
		const ObjectUIDType* ptrChildren;

		~RAWDATA()
		{
			ptrPivots = nullptr;
			ptrChildren = nullptr;
		}

		RAWDATA(const char* szData)
		{
			nUID = szData[0];
			nTotalPivots = *reinterpret_cast<const uint16_t*>(&szData[1]);
			ptrPivots = reinterpret_cast<const KeyType*>(szData + sizeof(uint8_t) + sizeof(uint16_t));
			ptrChildren = reinterpret_cast<const ObjectUIDType*>(szData + sizeof(uint8_t) + sizeof(uint16_t) + (nTotalPivots * sizeof(KeyType)));
		}
	};
END_PACKED_STRUCT

public:
	// Static UID to identify the type of the node
	static const uint8_t UID = TYPE_UID;

private:
	typedef IndexNodeROpt<KeyType, ValueType, ObjectUIDType, DataNodeType, UID> SelfType;

	typedef std::vector<KeyType>::const_iterator KeyTypeIterator;
	typedef std::vector<ObjectUIDType>::const_iterator CacheKeyTypeIterator;

private:
	// Vector to store pivot keys and child node UIDs
	std::vector<KeyType> m_vtPivots;
	std::vector<ObjectUIDType> m_vtChildren;

	const RAWDATA* m_ptrRawData = nullptr;
public:
	// Destructor: Clears pivot and child vectors
	~IndexNodeROpt()
	{
		m_vtPivots.clear();
		m_vtChildren.clear();

		delete m_ptrRawData;
	}

	// Default constructor
	IndexNodeROpt()
		: m_ptrRawData(nullptr)
	{
	}

	// Copy constructor: Copies pivot keys and child UIDs from another IndexNodeROpt
	IndexNodeROpt(const IndexNodeROpt& source)
		: m_ptrRawData(nullptr)
	{
		m_vtPivots.assign(source.m_vtPivots.begin(), source.m_vtPivots.end());
		m_vtChildren.assign(source.m_vtChildren.begin(), source.m_vtChildren.end());
	}

	// Constructor that deserializes the node from raw data
	IndexNodeROpt(const char* szData)
		: m_ptrRawData(nullptr)
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<typename ObjectUIDType::NodeUID>::value)
		{
			m_ptrRawData = new RAWDATA(szData);

			assert(UID == m_ptrRawData->nUID);

			moveDataToDRAM();
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

	// Constructor that deserializes the node from a file stream
	IndexNodeROpt(std::fstream& fs)
		: m_ptrRawData(nullptr)
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<typename ObjectUIDType::NodeUID>::value)
		{
			uint16_t nPivotCount;// , nValueCount;
			fs.read(reinterpret_cast<char*>(&nPivotCount), sizeof(uint16_t));
			//fs.read(reinterpret_cast<char*>(&nValueCount), sizeof(uint16_t));

			m_vtPivots.resize(nPivotCount);
			m_vtChildren.resize(nPivotCount + 1);

			fs.read(reinterpret_cast<char*>(m_vtPivots.data()), nPivotCount * sizeof(KeyType));
			fs.read(reinterpret_cast<char*>(m_vtChildren.data()), (nPivotCount + 1) * sizeof(typename ObjectUIDType::NodeUID));
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

	// Constructor that initializes the node with iterators over pivot keys and children
	IndexNodeROpt(KeyTypeIterator itBeginPivots, KeyTypeIterator itEndPivots, CacheKeyTypeIterator itBeginChildren, CacheKeyTypeIterator itEndChildren)
		: m_ptrRawData(nullptr)
	{
		m_vtPivots.assign(itBeginPivots, itEndPivots);
		m_vtChildren.assign(itBeginChildren, itEndChildren);
	}

	// Constructor that creates an internal node with a pivot key and two child UIDs
	IndexNodeROpt(const KeyType& pivotKey, const ObjectUIDType& ptrLHSNode, const ObjectUIDType& ptrRHSNode)
		: m_ptrRawData(nullptr)
	{
		m_vtPivots.push_back(pivotKey);
		m_vtChildren.push_back(ptrLHSNode);
		m_vtChildren.push_back(ptrRHSNode);
	}

public:
	inline void moveDataToDRAM()
	{
		if (m_ptrRawData != nullptr)
		{
			m_vtPivots.resize(m_ptrRawData->nTotalPivots);
			m_vtChildren.resize(m_ptrRawData->nTotalPivots + 1);

			memcpy(m_vtPivots.data(), m_ptrRawData->ptrPivots, m_ptrRawData->nTotalPivots * sizeof(KeyType));
			memcpy(m_vtChildren.data(), m_ptrRawData->ptrChildren, (m_ptrRawData->nTotalPivots + 1) * sizeof(typename ObjectUIDType::NodeUID));

			assert(m_ptrRawData->nUID == UID);
			assert(m_ptrRawData->nTotalPivots == m_vtPivots.size());
			for (int idx = 0, end = m_ptrRawData->nTotalPivots; idx < end; idx++)
			{
				assert(*(m_ptrRawData->ptrPivots + idx) == m_vtPivots[idx]);
				assert(*(m_ptrRawData->ptrChildren + idx) == m_vtChildren[idx]);
				assert(*(m_ptrRawData->ptrChildren + idx + 1) == m_vtChildren[idx + 1]);
			}

			delete m_ptrRawData;
			m_ptrRawData = nullptr;
		}
		else
		{
			throw new std::logic_error(".....");
		}
	}

	// Writes the node's data to a file stream
	inline void writeToStream(std::fstream& fs, uint8_t& uidObjectType, uint32_t& nDataSize) const
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<typename ObjectUIDType::NodeUID>::value)
		{
			uidObjectType = SelfType::UID;

			uint16_t nPivotCount = m_vtPivots.size();
			//uint16_t nValueCount = m_vtChildren.size();

			nDataSize = sizeof(uint8_t)					// UID
				+ sizeof(uint16_t)						// Total keys
			//	+ sizeof(uint16_t)						// Total values
				+ (nPivotCount * sizeof(KeyType))			// Size of all keys
				+ ( (nPivotCount + 1) * sizeof(typename ObjectUIDType::NodeUID));	// Size of all values

			fs.write(reinterpret_cast<const char*>(&uidObjectType), sizeof(uint8_t));
			fs.write(reinterpret_cast<const char*>(&nPivotCount), sizeof(uint16_t));
			//fs.write(reinterpret_cast<const char*>(&nValueCount), sizeof(uint16_t));
			fs.write(reinterpret_cast<const char*>(m_vtPivots.data()), nPivotCount * sizeof(KeyType));
			fs.write(reinterpret_cast<const char*>(m_vtChildren.data()), (nPivotCount + 1) * sizeof(typename ObjectUIDType::NodeUID));	// fix it!

#ifdef _DEBUG			
			for (auto it = m_vtChildren.begin(); it != m_vtChildren.end(); it++)
			{
				assert((*it).getMediaType() < 3);
			}
#endif _DEBUG
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

	// Serializes the node's data into a char buffer
	inline void serialize(char*& szBuffer, uint8_t& uidObjectType, uint32_t& nBufferSize) const
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<typename ObjectUIDType::NodeUID>::value &&
			std::is_standard_layout<typename ObjectUIDType::NodeUID>::value)
		{
			uidObjectType = UID;

			uint16_t nPivotCount = m_vtPivots.size();
			//uint16_t nValueCount = m_vtChildren.size();

			nBufferSize = sizeof(uint8_t)				// UID
				+ sizeof(uint16_t)						// Total keys
			//	+ sizeof(uint16_t)						// Total values
				+ (nPivotCount * sizeof(KeyType))			// Size of all keys
				+ ( (nPivotCount +1)* sizeof(typename ObjectUIDType::NodeUID));	// Size of all values

			szBuffer = new char[nBufferSize + 1];
			memset(szBuffer, 0, nBufferSize + 1);

			size_t nOffset = 0;
			memcpy(szBuffer, &uidObjectType, sizeof(uint8_t));
			nOffset += sizeof(uint8_t);

			memcpy(szBuffer + nOffset, &nPivotCount, sizeof(uint16_t));
			nOffset += sizeof(uint16_t);

			//memcpy(szBuffer + nOffset, &nValueCount, sizeof(uint16_t));
			//nOffset += sizeof(uint16_t);

			size_t nKeysSize = nPivotCount * sizeof(KeyType);
			memcpy(szBuffer + nOffset, m_vtPivots.data(), nKeysSize);
			nOffset += nKeysSize;

			size_t nValuesSize = (nPivotCount + 1) * sizeof(typename ObjectUIDType::NodeUID);
			memcpy(szBuffer + nOffset, m_vtChildren.data(), nValuesSize);
			//nOffset += nValuesSize;

			const RAWDATA* ptrRawData = new RAWDATA(szBuffer);
			assert(ptrRawData->nUID == UID);
			assert(ptrRawData->nTotalPivots == m_vtPivots.size());
			for (int idx = 0, end = ptrRawData->nTotalPivots; idx < end; idx++)
			{
				assert(*(ptrRawData->ptrPivots + idx) == m_vtPivots[idx]);
				assert(*(ptrRawData->ptrChildren + idx) == m_vtChildren[idx]);
				assert(*(ptrRawData->ptrChildren + idx + 1) == m_vtChildren[idx + 1]);
			}

#ifdef _DEBUG			
			for (auto it = m_vtChildren.begin(); it != m_vtChildren.end(); it++)
			{
				assert((*it).getMediaType() < 3);
			}
#endif _DEBUG
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

public:
	// Returns the number of keys (pivots) in the node
	inline size_t getKeysCount() const
	{
		return m_vtPivots.size();
	}

	// Finds the index of the child node for the given key
	inline size_t getChildNodeIdx(const KeyType& key) const
	{
		auto it = std::upper_bound(m_vtPivots.begin(), m_vtPivots.end(), key);
		return std::distance(m_vtPivots.begin(), it);

		//size_t nChildIdx = 0;
		//while (nChildIdx < m_vtPivots.size() && key >= m_vtPivots[nChildIdx])
		//{
		//	nChildIdx++;
		//}

		//assert(val == nChildIdx);

		//return val;
	}

	// Gets the child at the given index
	inline const ObjectUIDType& getChildAt(size_t nIdx) const
	{
		return m_vtChildren[nIdx];
	}

	// Gets the child node corresponding to the given key
	inline const ObjectUIDType& getChild(const KeyType& key) const
	{
		return m_vtChildren[getChildNodeIdx(key)];
	}

	// Returns the first pivot key
	inline const KeyType& getFirstChild() const
	{
		return m_vtPivots[0];
	}

	// Checks if the node requires a split based on the degree
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

	inline size_t getSize() const
	{
		if constexpr (std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<ValueType>::value &&
			std::is_standard_layout<ValueType>::value)
		{
			return sizeof(uint8_t)
				+ sizeof(size_t)
				+ sizeof(size_t)
				+ (m_vtPivots.size() * sizeof(KeyType))
				+ (m_vtChildren.size() * sizeof(typename ObjectUIDType::NodeUID));
		}
		else
		{
			static_assert(
				std::is_trivial<KeyType>::value &&
				std::is_standard_layout<KeyType>::value &&
				std::is_trivial<ValueType>::value &&
				std::is_standard_layout<ValueType>::value,
				"Non-POD type is provided. Kindly provide functionality to calculate size.");
		}
	}

	template <typename CacheObjectType>
	void updateChildUID(std::shared_ptr<CacheObjectType> ptrChildNode, const ObjectUIDType& uidOld, const ObjectUIDType& uidNew)
	{
		const KeyType* key = nullptr;
		if (std::holds_alternative<std::shared_ptr<SelfType>>(ptrChildNode->getInnerData()))
		{
			std::shared_ptr<SelfType> ptrIndexNode = std::get<std::shared_ptr<SelfType>>(ptrChildNode->getInnerData());
			key = &ptrIndexNode->getFirstChild();
		}
		else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(ptrChildNode->getInnerData()))
		{
			std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(ptrChildNode->getInnerData());
			key = &ptrDataNode->getFirstChild();
		}

		auto it = std::upper_bound(m_vtPivots.begin(), m_vtPivots.end(), *key);
		auto index = std::distance(m_vtPivots.begin(), it);

		assert(m_vtChildren[index] == uidOld);

		m_vtChildren[index] = uidNew;

		return;
		//int idx = 0;
		//auto it = m_vtChildren.begin();
		//while (it != m_vtChildren.end())
		//{
		//	if (*it == uidOld)
		//	{
		//		*it = uidNew;
		//		return;
		//	}
		//	it++;
		//}

		//throw new std::logic_error("should not occur!");
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
	inline ErrorCode insert(const KeyType& pivotKey, const ObjectUIDType& uidSibling)
	{
		auto it = std::lower_bound(m_vtPivots.begin(), m_vtPivots.end(), pivotKey);
		auto nChildIdx = std::distance(m_vtPivots.begin(), it);

		//size_t nChildIdx_ = m_vtPivots.size();
		//for (int nIdx = 0; nIdx < m_vtPivots.size(); ++nIdx)
		//{
		//	if (pivotKey < m_vtPivots[nIdx])
		//	{
		//		nChildIdx_ = nIdx;
		//		break;
		//	}
		//}

		//assert(nChildIdx == nChildIdx_);

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

		ObjectTypePtr ptrLHSStorageObject = nullptr;
		ObjectTypePtr ptrRHSStorageObject = nullptr;

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
			ptrCache->template getObjectOfType<std::shared_ptr<SelfType>>(m_vtChildren[nChildIdx - 1], ptrLHSNode, ptrLHSStorageObject, uidUpdated);    //TODO: lock
#else __TREE_WITH_CACHE__
			ptrCache->template getObjectOfType<std::shared_ptr<SelfType>>(m_vtChildren[nChildIdx - 1], ptrLHSNode, ptrLHSStorageObject);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(ptrLHSStorageObject->getMutex());
#endif __CONCURRENT__

#ifdef __TREE_WITH_CACHE__
			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nChildIdx - 1] = *uidUpdated;
			}

			ptrLHSStorageObject->setDirtyFlag(true);

			uidAffectedNode = m_vtChildren[nChildIdx - 1];
			ptrAffectedNode = ptrLHSStorageObject;
#endif __TREE_WITH_CACHE__

			if (ptrLHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))	// TODO: macro?
			{
				KeyType key;
				ptrChild->moveAnEntityFromLHSSibling(ptrLHSNode, m_vtPivots[nChildIdx - 1], key);

				m_vtPivots[nChildIdx - 1] = key;
				return ErrorCode::Success;
			}

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
#ifdef __TREE_WITH_CACHE__
			std::optional<ObjectUIDType> uidUpdated = std::nullopt;
			ptrCache->template getObjectOfType<std::shared_ptr<SelfType>>(m_vtChildren[nChildIdx + 1], ptrRHSNode, ptrRHSStorageObject, uidUpdated);    //TODO: lock
#else __TREE_WITH_CACHE__
			ptrCache->template getObjectOfType<std::shared_ptr<SelfType>>(m_vtChildren[nChildIdx + 1], ptrRHSNode, ptrRHSStorageObject);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(ptrRHSStorageObject->getMutex());
#endif __CONCURRENT__

#ifdef __TREE_WITH_CACHE__
			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nChildIdx + 1] = *uidUpdated;
			}

			ptrRHSStorageObject->setDirtyFlag(true);

			uidAffectedNode = m_vtChildren[nChildIdx + 1];
			ptrAffectedNode = ptrRHSStorageObject;
#endif __TREE_WITH_CACHE__

			if (ptrRHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromRHSSibling(ptrRHSNode, m_vtPivots[nChildIdx], key);

				m_vtPivots[nChildIdx] = key;
				return ErrorCode::Success;
			}

			ptrChild->mergeNodes(ptrRHSNode, m_vtPivots[nChildIdx]);

			assert(uidChild == m_vtChildren[nChildIdx]);

			uidObjectToDelete = m_vtChildren[nChildIdx + 1];

			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx);
			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}

		//		if (nChildIdx > 0)
		//		{
		//#ifdef __CONCURRENT__
		//			std::unique_lock<std::shared_mutex> lock(ptrLHSStorageObject->getMutex());	//Lock acquired twice!!! merge the respective sections!
		//#endif __CONCURRENT__
		//
		//			ptrLHSNode->mergeNodes(ptrChild, m_vtPivots[nChildIdx - 1]);
		//
		//			uidObjectToDelete = m_vtChildren[nChildIdx];
		//			if (uidObjectToDelete != uidChild)
		//			{
		//				throw new std::logic_error("should not occur!");
		//			}
		//
		//			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx - 1);
		//			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx);
		//
		//			//uidObjectToDelete = uidChild;
		//
		//			return ErrorCode::Success;
		//		}
		//
		//		if (nChildIdx < m_vtPivots.size())
		//		{
		//#ifdef __CONCURRENT__
		//			std::unique_lock<std::shared_mutex> lock(ptrRHSStorageObject->getMutex());
		//#endif __CONCURRENT__
		//
		//			ptrChild->mergeNodes(ptrRHSNode, m_vtPivots[nChildIdx]);
		//
		//			assert(uidChild == m_vtChildren[nChildIdx]);
		//
		//			uidObjectToDelete = m_vtChildren[nChildIdx + 1];
		//
		//			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx);
		//			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx + 1);
		//
		//			return ErrorCode::Success;
		//		}

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

		ObjectTypePtr ptrLHSStorageObject = nullptr;
		ObjectTypePtr ptrRHSStorageObject = nullptr;

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
			ptrCache->template getObjectOfType<std::shared_ptr<DataNodeType>>(m_vtChildren[nChildIdx - 1], ptrLHSNode, ptrLHSStorageObject, uidUpdated);    //TODO: lock
#else __TREE_WITH_CACHE__
			ptrCache->template getObjectOfType<std::shared_ptr<DataNodeType>>(m_vtChildren[nChildIdx - 1], ptrLHSNode, ptrLHSStorageObject);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(ptrLHSStorageObject->getMutex());
#endif __CONCURRENT__

#ifdef __TREE_WITH_CACHE__
			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nChildIdx - 1] = *uidUpdated;
			}

			ptrLHSStorageObject->setDirtyFlag(true);

			uidAffectedNode = m_vtChildren[nChildIdx - 1];
			ptrAffectedNode = ptrLHSStorageObject;
#endif __TREE_WITH_CACHE__

			if (ptrLHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromLHSSibling(ptrLHSNode, key);

				m_vtPivots[nChildIdx - 1] = key;

				return ErrorCode::Success;
			}

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
#ifdef __TREE_WITH_CACHE__
			std::optional<ObjectUIDType> uidUpdated = std::nullopt;
			ptrCache->template getObjectOfType<std::shared_ptr<DataNodeType>>(m_vtChildren[nChildIdx + 1], ptrRHSNode, ptrRHSStorageObject, uidUpdated);    //TODO: lock
#else __TREE_WITH_CACHE__
			ptrCache->template getObjectOfType<std::shared_ptr<DataNodeType>>(m_vtChildren[nChildIdx + 1], ptrRHSNode, ptrRHSStorageObject);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
			std::unique_lock<std::shared_mutex> lock(ptrRHSStorageObject->getMutex());
#endif __CONCURRENT__

#ifdef __TREE_WITH_CACHE__
			if (uidUpdated != std::nullopt)
			{
				m_vtChildren[nChildIdx + 1] = *uidUpdated;
			}

			ptrRHSStorageObject->setDirtyFlag(true);

			uidAffectedNode = m_vtChildren[nChildIdx + 1];
			ptrAffectedNode = ptrRHSStorageObject;
#endif __TREE_WITH_CACHE__

			if (ptrRHSNode->getKeysCount() > std::ceil(nDegree / 2.0f))
			{
				KeyType key;
				ptrChild->moveAnEntityFromRHSSibling(ptrRHSNode, key);

				m_vtPivots[nChildIdx] = key;
				return ErrorCode::Success;
			}

			ptrChild->mergeNode(ptrRHSNode);

			uidObjectToDelete = m_vtChildren[nChildIdx + 1];

			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx);
			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx + 1);

			return ErrorCode::Success;
		}

		//		if (nChildIdx > 0)
		//		{
		//#ifdef __CONCURRENT__
		//			std::unique_lock<std::shared_mutex> lock(ptrLHSStorageObject->getMutex());
		//#endif __CONCURRENT__
		//
		//			ptrLHSNode->mergeNode(ptrChild);
		//
		//			uidObjectToDelete = m_vtChildren[nChildIdx];
		//			if (uidObjectToDelete != uidChild)
		//			{
		//				throw new std::logic_error("should not occur!");
		//			}
		//
		//			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx - 1);
		//			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx);
		//
		//			//uidObjectToDelete = uidChild;
		//
		//			return ErrorCode::Success;
		//		}
		//
		//		if (nChildIdx < m_vtPivots.size())
		//		{
		//#ifdef __CONCURRENT__
		//			std::unique_lock<std::shared_mutex> lock(ptrRHSStorageObject->getMutex());
		//#endif __CONCURRENT__
		//
		//			ptrChild->mergeNode(ptrRHSNode);
		//
		//			uidObjectToDelete = m_vtChildren[nChildIdx + 1];
		//
		//			m_vtPivots.erase(m_vtPivots.begin() + nChildIdx);
		//			m_vtChildren.erase(m_vtChildren.begin() + nChildIdx + 1);
		//
		//			return ErrorCode::Success;
		//		}

		throw new logic_error("should not occur!"); // TODO: critical log entry.
	}

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
	template <typename CacheType, typename CacheObjectType>
	void print(std::ofstream& os, std::shared_ptr<CacheType>& ptrCache, size_t nLevel, string stPrefix)
	{
		uint8_t nSpaceCount = 7;

		stPrefix.append(std::string(nSpaceCount - 1, ' '));
		stPrefix.append("|");
		for (size_t nIndex = 0; nIndex < m_vtChildren.size(); nIndex++)
		{
			os
				<< " "
				<< stPrefix
				<< std::endl;

			os
				<< " "
				<< stPrefix
				<< std::string(nSpaceCount, '-').c_str();

			if (nIndex < m_vtPivots.size())
			{
				os
					<< " < ("
					<< m_vtPivots[nIndex] << ")";
			}
			else
			{
				os
					<< " >= ("
					<< m_vtPivots[nIndex - 1]
					<< ")";
			}

			CacheObjectType ptrNode = nullptr;

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

			os << std::endl;

			if (std::holds_alternative<shared_ptr<SelfType>>(ptrNode->getInnerData()))
			{
				shared_ptr<SelfType> ptrIndexNode = std::get<shared_ptr<SelfType>>(ptrNode->getInnerData());
				ptrIndexNode->template print<CacheType, CacheObjectType>(os, ptrCache, nLevel + 1, stPrefix);
			}
			else //if (std::holds_alternative<shared_ptr<DataNodeType>>(ptrNode->getInnerData()))
			{
				shared_ptr<DataNodeType> ptrDataNode = std::get<shared_ptr<DataNodeType>>(ptrNode->getInnerData());
				ptrDataNode->print(os, nLevel + 1, stPrefix);
			}
		}
	}

	void wieHiestDu()
	{
		printf("ich heisse InternalNode.\n");
	}
};