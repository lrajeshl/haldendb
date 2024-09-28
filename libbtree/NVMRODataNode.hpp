#pragma once
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <optional>

#include <iostream>
#include <fstream>
#include <assert.h>
#include "ErrorCodes.h"

template <typename KeyType, typename ValueType, typename ObjectUIDType, typename NVMDataNode, typename DRAMDataNode, uint8_t TYPE_UID>
class NVMRODataNode
{
public:
	static const uint8_t UID = TYPE_UID;

private:
	typedef NVMRODataNode<KeyType, ValueType, ObjectUIDType, NVMDataNode, DRAMDataNode, TYPE_UID> SelfType;

	typedef std::vector<KeyType>::const_iterator KeyTypeIterator;
	typedef std::vector<ValueType>::const_iterator ValueTypeIterator;

public:
	std::shared_ptr<DRAMDataNode> m_ptrDRAMDataNode;
	std::shared_ptr<const NVMDataNode> m_ptrNVMDataNode;

public:
	~NVMRODataNode()
	{
	}

	NVMRODataNode()
		: m_ptrNVMDataNode(nullptr)
	{
		m_ptrDRAMDataNode = std::make_shared<DRAMDataNode>();
	}

	NVMRODataNode(const NVMRODataNode& source)
		: m_ptrNVMDataNode(nullptr)
		, m_ptrDRAMDataNode(nullptr)
	{
		throw new std::logic_error("implement the logic!");
	}

	NVMRODataNode(const char* szData)
		: m_ptrDRAMDataNode(nullptr)
	{
		m_ptrNVMDataNode = std::make_shared<NVMDataNode>(szData, true);
	}

	NVMRODataNode(std::fstream& is)
		: m_ptrDRAMDataNode(nullptr)
	{
		m_ptrNVMDataNode = std::make_shared<NVMDataNode>(is);
	}

	inline void moveDataToDRAM() 
	{
		if (m_ptrDRAMDataNode == nullptr)
		{
			if (m_ptrNVMDataNode != nullptr)
			{
				m_ptrDRAMDataNode = std::make_shared<DRAMDataNode>(
					m_ptrNVMDataNode->m_vtKeys.begin(), m_ptrNVMDataNode->m_vtKeys.end(),
					m_ptrNVMDataNode->m_vtValues.begin(), m_ptrNVMDataNode->m_vtValues.end());

				m_ptrNVMDataNode.reset();
			}
			else {
				m_ptrDRAMDataNode = std::make_shared<DRAMDataNode>();
			}
		}
	}

	inline ErrorCode insert(const KeyType& key, const ValueType& value)
	{
		moveDataToDRAM();
		return m_ptrDRAMDataNode->insert(key, value);
	}

	inline ErrorCode remove(const KeyType& key)
	{
		moveDataToDRAM();
		return m_ptrDRAMDataNode->remove(key);
	}

	inline bool requireSplit(size_t nDegree) const
	{
		if (m_ptrNVMDataNode != nullptr)
			return m_ptrNVMDataNode->requireSplit(nDegree);

		return m_ptrDRAMDataNode->requireSplit(nDegree);
	}

	inline bool requireMerge(size_t nDegree)
	{
		if (m_ptrNVMDataNode != nullptr)
			return m_ptrNVMDataNode->requireMerge(nDegree);

		return m_ptrDRAMDataNode->requireMerge(nDegree);
	}

	inline size_t getKeysCount() 
	{
		if (m_ptrNVMDataNode != nullptr)
			return m_ptrNVMDataNode->getKeysCount();

		return m_ptrDRAMDataNode->getKeysCount();
	}

	inline ErrorCode getValue(const KeyType& key, ValueType& value) const
	{
		if (m_ptrNVMDataNode != nullptr)
			return m_ptrNVMDataNode->getValue(key, value);

		return m_ptrDRAMDataNode->getValue(key, value);
	}

	template <typename CacheType, typename CacheObjectTypePtr>
	inline ErrorCode split(std::shared_ptr<CacheType>& ptrCache, std::optional<ObjectUIDType>& uidSibling, CacheObjectTypePtr& ptrSibling, KeyType& pivotKeyForParent)
	{
		//std::shared_ptr<SelfType> ptrSibling = nullptr;
		ptrCache->template createObjectOfType<SelfType>(uidSibling, ptrSibling);

		if (!uidSibling)
		{
			//delete node!
			return ErrorCode::Error;
		}

		moveDataToDRAM();

		std::shared_ptr<SelfType> ptrInnerData = std::get<std::shared_ptr<SelfType>>(ptrSibling->getInnerData());

		return m_ptrDRAMDataNode->split(ptrInnerData->m_ptrDRAMDataNode, pivotKeyForParent);
	}

	inline void moveAnEntityFromLHSSibling(std::shared_ptr<SelfType> ptrLHSSibling, KeyType& pivotKeyForParent)
	{
		moveDataToDRAM();
		ptrLHSSibling->moveDataToDRAM();

		return m_ptrDRAMDataNode->moveAnEntityFromLHSSibling(ptrLHSSibling->m_ptrDRAMDataNode, pivotKeyForParent);
	}

	inline void moveAnEntityFromRHSSibling(std::shared_ptr<SelfType> ptrRHSSibling, KeyType& pivotKeyForParent)
	{
		moveDataToDRAM();
		ptrRHSSibling->moveDataToDRAM();

		return m_ptrDRAMDataNode->moveAnEntityFromRHSSibling(ptrRHSSibling->m_ptrDRAMDataNode, pivotKeyForParent);
	}

	inline void mergeNode(std::shared_ptr<SelfType> ptrSibling)
	{
		moveDataToDRAM();
		ptrSibling->moveDataToDRAM();

		return m_ptrDRAMDataNode->mergeNode(ptrSibling->m_ptrDRAMDataNode);
	}

public:
	inline size_t getSize()
	{
		if (m_ptrNVMDataNode != nullptr)
			return m_ptrNVMDataNode->getSize();

		return m_ptrDRAMDataNode->getSize();
	}

	inline void serialize(char*& szBuffer, uint8_t& uidObjectType, size_t& nBufferSize)
	{
		if (m_ptrNVMDataNode != nullptr)
			return m_ptrNVMDataNode->serialize(szBuffer, uidObjectType, nBufferSize);

		return m_ptrDRAMDataNode->serialize(szBuffer, uidObjectType, nBufferSize);
	}

	inline void writeToStream(std::fstream& os, uint8_t& uidObjectType, size_t& nDataSize)
	{
		if (m_ptrNVMDataNode != nullptr)
			return m_ptrNVMDataNode->writeToStream(os, uidObjectType, nDataSize);

		return m_ptrDRAMDataNode->writeToStream(os, uidObjectType, nDataSize);
	}

public:
	void print(std::ofstream& out, size_t nLevel, std::string prefix)
	{
		if (m_ptrNVMDataNode != nullptr)
			return m_ptrNVMDataNode->print(out, nLevel, prefix);

		return m_ptrDRAMDataNode->print(out, nLevel, prefix);
	}

	void wieHiestDu() {
		printf("ich heisse NVMRODataNode :).\n");
	}
};