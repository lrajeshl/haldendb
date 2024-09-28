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

template <typename KeyType, typename ValueType, typename ObjectUIDType, typename NVMIndexNode, typename DRAMIndexNode, typename DataNodeType, uint8_t TYPE_UID>
class NVMROIndexNode
{
public:
	static const uint8_t UID = TYPE_UID;
	
private:
	typedef NVMROIndexNode<KeyType, ValueType, ObjectUIDType, NVMIndexNode, DRAMIndexNode, DataNodeType, TYPE_UID> SelfType;

	typedef std::vector<KeyType>::const_iterator KeyTypeIterator;
	typedef std::vector<ObjectUIDType>::const_iterator CacheKeyTypeIterator;

public:
	std::shared_ptr<DRAMIndexNode> m_ptrDRAMIndexNode;
	std::shared_ptr<const NVMIndexNode> m_ptrNVMIndexNode;

public:
	~NVMROIndexNode()
	{
	}

	NVMROIndexNode()
		: m_ptrNVMIndexNode(nullptr)
	{
		m_ptrDRAMIndexNode = std::make_shared<DRAMIndexNode>();
	}

	NVMROIndexNode(const NVMROIndexNode& source)
		: m_ptrNVMIndexNode(nullptr)
		, m_ptrDRAMIndexNode(nullptr)
	{
		throw new std::logic_error("implement the logic!");
	}

	NVMROIndexNode(const char* szData)
		: m_ptrDRAMIndexNode(nullptr)
	{
		m_ptrNVMIndexNode = std::make_shared<NVMIndexNode>(szData, true);
	}

	NVMROIndexNode(std::fstream& is)
		: m_ptrDRAMIndexNode(nullptr)
	{
		m_ptrNVMIndexNode = std::make_shared<NVMIndexNode>(is);
	}

	NVMROIndexNode(const KeyType& pivotKey, const ObjectUIDType& ptrLHSNode, const ObjectUIDType& ptrRHSNode)
		: m_ptrNVMIndexNode(nullptr)
	{
		m_ptrDRAMIndexNode = std::make_shared<DRAMIndexNode>(pivotKey, ptrLHSNode, ptrRHSNode);
	}


	inline void moveDataToDRAM()
	{
		if (m_ptrDRAMIndexNode == nullptr)
		{
			if (m_ptrNVMIndexNode != nullptr)
			{
				m_ptrDRAMIndexNode = std::make_shared<DRAMIndexNode>(
					m_ptrNVMIndexNode->m_vtPivots.begin(), m_ptrNVMIndexNode->m_vtPivots.end(),
					m_ptrNVMIndexNode->m_vtChildren.begin(), m_ptrNVMIndexNode->m_vtChildren.end());

				m_ptrNVMIndexNode.reset();
			}
			else {
				m_ptrDRAMIndexNode = std::make_shared<DRAMIndexNode>();
			}
		}
	}

	inline ErrorCode insert(const KeyType& pivotKey, const ObjectUIDType& uidSibling)
	{
		moveDataToDRAM();
		return m_ptrDRAMIndexNode->insert(pivotKey, uidSibling);
	}

#ifdef __TREE_WITH_CACHE__
	template <typename CacheType>
	inline ErrorCode rebalanceIndexNode(std::shared_ptr<CacheType>& ptrCache, const ObjectUIDType& uidChild, std::shared_ptr<SelfType>& ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete, std::optional<ObjectUIDType>& uidAffectedNode, CacheType::ObjectTypePtr& ptrAffectedNode)
#else __TREE_WITH_CACHE__
	template <typename CacheType, typename ObjectCoreType>
	inline ErrorCode rebalanceIndexNode(std::shared_ptr<CacheType>& ptrCache, const ObjectUIDType& uidChild, std::shared_ptr<SelfType>& ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete)
#endif __TREE_WITH_CACHE__
	{
		moveDataToDRAM();

#ifdef __TREE_WITH_CACHE__
		return m_ptrDRAMIndexNode->template rebalanceIndexNode<CacheType>(ptrCache, uidChild, ptrChild->m_ptrDRAMIndexNode, key, nDegree, uidObjectToDelete, uidAffectedNode, ptrAffectedNode);
#else __TREE_WITH_CACHE__
		return m_ptrDRAMIndexNode->rebalanceIndexNode(ptrCache, uidChild, ptrChild, key, nDegree, uidObjectToDelete);
#endif __TREE_WITH_CACHE__
	}

#ifdef __TREE_WITH_CACHE__
	template <typename CacheType>
	inline ErrorCode rebalanceDataNode(std::shared_ptr<CacheType>& ptrCache, const ObjectUIDType& uidChild, std::shared_ptr<DataNodeType>& ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete, std::optional<ObjectUIDType>& uidAffectedNode, CacheType::ObjectTypePtr& ptrAffectedNode)
#else __TREE_WITH_CACHE__
	template <typename CacheType, typename ObjectCoreType>
	inline ErrorCode rebalanceDataNode(std::shared_ptr<CacheType>& ptrCache, const ObjectUIDType& uidChild, std::shared_ptr<SelfType>& ptrChild, const KeyType& key, size_t nDegree, std::optional<ObjectUIDType>& uidObjectToDelete)
#endif __TREE_WITH_CACHE__
	{
		moveDataToDRAM();
#ifdef __TREE_WITH_CACHE__
		return m_ptrDRAMIndexNode->template rebalanceDataNode<CacheType>(ptrCache, uidChild, ptrChild->m_ptrDRAMDataNode, key, nDegree, uidObjectToDelete, uidAffectedNode, ptrAffectedNode);
#else __TREE_WITH_CACHE__
		return m_ptrDRAMIndexNode->rebalanceDataNode(ptrCache, uidChild, ptrChild, key, nDegree, uidObjectToDelete);
#endif __TREE_WITH_CACHE__
	}

	inline size_t getKeysCount() 
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->getKeysCount();

		return m_ptrDRAMIndexNode->getKeysCount();
	}

	inline size_t getChildNodeIdx(const KeyType& key)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->getChildNodeIdx(key);

		return m_ptrDRAMIndexNode->getChildNodeIdx(key);
	}

	inline ObjectUIDType getChildAt(size_t nIdx) 
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->getChildAt(nIdx);

		return m_ptrDRAMIndexNode->getChildAt(nIdx);
	}

	inline ObjectUIDType getChild(const KeyType& key)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->getChild(key);

		return m_ptrDRAMIndexNode->getChild(key);
	}

	inline bool requireSplit(size_t nDegree)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->requireSplit(nDegree);

		return m_ptrDRAMIndexNode->requireSplit(nDegree);
	}

	inline bool canTriggerSplit(size_t nDegree)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->canTriggerSplit(nDegree);

		return m_ptrDRAMIndexNode->canTriggerSplit(nDegree);
	}

	inline bool canTriggerMerge(size_t nDegree)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->canTriggerMerge(nDegree);

		return m_ptrDRAMIndexNode->canTriggerMerge(nDegree);
	}

	inline bool requireMerge(size_t nDegree)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->requireMerge(nDegree);

		return m_ptrDRAMIndexNode->requireMerge(nDegree);
	}

	template <typename CacheType, typename CacheObjectTypePtr>
	inline ErrorCode split(std::shared_ptr<CacheType>& ptrCache, std::optional<ObjectUIDType>& uidSibling, CacheObjectTypePtr& ptrSiblingy, KeyType& pivotKeyForParent)
	{
		std::shared_ptr<SelfType> ptrSibling = nullptr;
		ptrCache->template createObjectOfType<SelfType>(uidSibling, ptrSibling);

		if (!uidSibling)
		{
			//delete node!
			return ErrorCode::Error;
		}

		moveDataToDRAM();

		return m_ptrDRAMIndexNode->split(ptrSibling->m_ptrDRAMIndexNode, pivotKeyForParent);
	}

	inline void moveAnEntityFromLHSSibling(shared_ptr<SelfType> ptrLHSSibling, KeyType& pivotKeyForEntity, KeyType& pivotKeyForParent)
	{
		moveDataToDRAM();
		ptrLHSSibling->moveDataToDRAM();

		return m_ptrDRAMIndexNode->moveAnEntityFromLHSSibling(ptrLHSSibling->m_ptrDRAMIndexNode, pivotKeyForEntity, pivotKeyForParent);
	}

	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& pivotKeyForEntity, KeyType& pivotKeyForParent)
	{
		moveDataToDRAM();
		ptrRHSSibling->moveDataToDRAM();

		return m_ptrDRAMIndexNode->moveAnEntityFromRHSSibling(ptrRHSSibling->m_ptrDRAMIndexNode, pivotKeyForEntity, pivotKeyForParent);
	}

	inline void mergeNodes(shared_ptr<SelfType> ptrSibling, KeyType& pivotKey)
	{
		moveDataToDRAM();
		ptrSibling->moveDataToDRAM();

		return m_ptrDRAMIndexNode->mergeNodes(ptrSibling->m_ptrDRAMIndexNode, pivotKey);
	}

public:
	inline void writeToStream(std::fstream& os, uint8_t& uidObjectType, size_t& nDataSize)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->writeToStream(os, uidObjectType, nDataSize);

		return m_ptrDRAMIndexNode->writeToStream(os, uidObjectType, nDataSize);
	}

	inline void serialize(char*& szBuffer, uint8_t& uidObjectType, size_t& nBufferSize)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->serialize(szBuffer, uidObjectType, nBufferSize);

		return m_ptrDRAMIndexNode->serialize(szBuffer, uidObjectType, nBufferSize);
	}

	inline size_t getSize()
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->getSize();

		return m_ptrDRAMIndexNode->getSize();
	}

	void updateChildUID(const ObjectUIDType& uidOld, const ObjectUIDType& uidNew)
	{
		moveDataToDRAM();
		return m_ptrDRAMIndexNode->updateChildUID(uidOld, uidNew);
	}


	inline std::vector<ObjectUIDType>::iterator getChildrenBeginIterator()
	{
		moveDataToDRAM();
		return m_ptrDRAMIndexNode->getChildrenBeginIterator();
	}

	inline std::vector<ObjectUIDType>::iterator getChildrenEndIterator()
	{
		moveDataToDRAM();
		return m_ptrDRAMIndexNode->getChildrenEndIterator();
	}

public:
	template <typename CacheType, typename ObjectType, typename DataNodeType>
	void print(std::ofstream& out, CacheType ptrCache, size_t nLevel, string prefix)
	{
		if (m_ptrNVMIndexNode != nullptr)
			return m_ptrNVMIndexNode->print(out, ptrCache, nLevel, prefix);

		return m_ptrDRAMIndexNode->print(out, ptrCache, nLevel, prefix);
	}

	void wieHiestDu() {
		printf("ich heisse InternalNode.\n");
	}
};
