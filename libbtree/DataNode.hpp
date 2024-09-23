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

template <typename KeyType, typename ValueType, typename ObjectUIDType, uint8_t TYPE_UID>
class DataNode
{
public:
	static const uint8_t UID = TYPE_UID;

private:
	typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID> SelfType;

	typedef std::vector<KeyType>::const_iterator KeyTypeIterator;
	typedef std::vector<ValueType>::const_iterator ValueTypeIterator;

	std::vector<KeyType> m_vtKeys;
	std::vector<ValueType> m_vtValues;

public:
	~DataNode()
	{
		m_vtKeys.clear();
		m_vtValues.clear();
	}

	DataNode()
	{
	}

	DataNode(const DataNode& source)
	{
		m_vtKeys.assign(source.m_vtKeys.begin(), source.m_vtKeys.end());
		m_vtValues.assign(source.m_vtValues.begin(), source.m_vtValues.end());
	}

	DataNode(const char* szData)
	{
		size_t nKeyCount, nValueCount = 0;

		size_t nOffset = sizeof(uint8_t);

		memcpy(&nKeyCount, szData + nOffset, sizeof(size_t));
		nOffset += sizeof(size_t);

		memcpy(&nValueCount, szData + nOffset, sizeof(size_t));
		nOffset += sizeof(size_t);

		m_vtKeys.resize(nKeyCount);
		m_vtValues.resize(nValueCount);

		size_t nKeysSize = nKeyCount * sizeof(KeyType);
		memcpy(m_vtKeys.data(), szData + nOffset, nKeysSize);

		//std::vector<KeyType> vt_ints(
		//	reinterpret_cast<const int*>(szData + nOffset),
		//	reinterpret_cast<const int*>(szData + nOffset + nKeyCount * sizeof(KeyType))
		//);;

		nOffset += nKeysSize;

		size_t nValuesSize = nValueCount * sizeof(ValueType);
		memcpy(m_vtValues.data(), szData + nOffset, nValuesSize);
	}

	DataNode(const char* szData, bool readonly)
	{
		size_t nKeyCount, nValueCount = 0;

		size_t nOffset = sizeof(uint8_t);

		memcpy(&nKeyCount, szData + nOffset, sizeof(size_t));
		nOffset += sizeof(size_t);

		memcpy(&nValueCount, szData + nOffset, sizeof(size_t));
		nOffset += sizeof(size_t);

		m_vtKeys.resize(nKeyCount);
		m_vtValues.resize(nValueCount);

		size_t nKeysSize = nKeyCount * sizeof(KeyType);
		//memcpy(m_vtKeys.data(), szData + nOffset, nKeysSize);

		m_vtKeys.assign(
			reinterpret_cast<const KeyType*>(szData + nOffset),
			reinterpret_cast<const KeyType*>(szData + nOffset + nKeysSize)
		);

		nOffset += nKeysSize;

		size_t nValuesSize = nValueCount * sizeof(ValueType);
		//memcpy(m_vtValues.data(), szData + nOffset, nValuesSize);
		m_vtValues.assign(
			reinterpret_cast<const ValueType*>(szData + nOffset),
			reinterpret_cast<const ValueType*>(szData + nOffset + nValuesSize)
		);
	}

	DataNode(std::fstream& is)
	{
		size_t keyCount, valueCount;

		is.read(reinterpret_cast<char*>(&keyCount), sizeof(size_t));
		is.read(reinterpret_cast<char*>(&valueCount), sizeof(size_t));

		m_vtKeys.resize(keyCount);
		m_vtValues.resize(valueCount);

		is.read(reinterpret_cast<char*>(m_vtKeys.data()), keyCount * sizeof(KeyType));
		is.read(reinterpret_cast<char*>(m_vtValues.data()), valueCount * sizeof(ValueType));
	}

	DataNode(KeyTypeIterator itBeginKeys, KeyTypeIterator itEndKeys, ValueTypeIterator itBeginValues, ValueTypeIterator itEndValues)
	{
		m_vtKeys.assign(itBeginKeys, itEndKeys);
		m_vtValues.assign(itBeginValues, itEndValues);
	}

	inline ErrorCode insert(const KeyType& key, const ValueType& value)
	{
		size_t nChildIdx = m_vtKeys.size();
		for (int nIdx = 0; nIdx < m_vtKeys.size(); ++nIdx)
		{
			if (key < m_vtKeys[nIdx])
			{
				nChildIdx = nIdx;
				break;
			}
		}

		m_vtKeys.insert(m_vtKeys.begin() + nChildIdx, key);
		m_vtValues.insert(m_vtValues.begin() + nChildIdx, value);

		return ErrorCode::Success;
	}

	inline ErrorCode remove(const KeyType& key)
	{
		KeyTypeIterator it = std::lower_bound(m_vtKeys.begin(), m_vtKeys.end(), key);

		if (it != m_vtKeys.end() && *it == key)
		{
			int index = it - m_vtKeys.begin();
			m_vtKeys.erase(it);
			m_vtValues.erase(m_vtValues.begin() + index);

			return ErrorCode::Success;
		}

		return ErrorCode::KeyDoesNotExist;
	}

	inline bool requireSplit(size_t nDegree) const
	{
		return m_vtKeys.size() > nDegree;
	}

	inline bool requireMerge(size_t nDegree) const
	{
		return m_vtKeys.size() <= std::ceil(nDegree / 2.0f);
	}

	inline size_t getKeysCount() const {
		return m_vtKeys.size();
	}

	//inline ErrorCode getValue(const KeyType& key, ValueType& value)
	//{
	//	KeyTypeIterator it = std::lower_bound(m_vtKeys.begin(), m_vtKeys.end(), key);
	//	if (it != m_vtKeys.end() && *it == key)
	//	{
	//		size_t index = it - m_vtKeys.begin();
	//		value = m_vtValues[index];

	//		return ErrorCode::Success;
	//	}

	//	return ErrorCode::KeyDoesNotExist;
	//}

	inline ErrorCode getValue(const KeyType& key, ValueType& value) const
	{
		KeyTypeIterator it = std::lower_bound(m_vtKeys.begin(), m_vtKeys.end(), key);
		if (it != m_vtKeys.end() && *it == key)
		{
			size_t index = it - m_vtKeys.begin();
			value = m_vtValues[index];

			return ErrorCode::Success;
		}

		return ErrorCode::KeyDoesNotExist;
	}

	template <typename CacheType, typename CacheObjectTypePtr>
	inline ErrorCode split(std::shared_ptr<CacheType>& ptrCache, std::optional<ObjectUIDType>& uidSibling, CacheObjectTypePtr& ptrSibling, KeyType& pivotKeyForParent)
	{
		size_t nMid = m_vtKeys.size() / 2;

		ptrCache->template createObjectOfType<SelfType>(uidSibling, ptrSibling,
			m_vtKeys.begin() + nMid, m_vtKeys.end(),
			m_vtValues.begin() + nMid, m_vtValues.end());

		if (!uidSibling)
		{
			return ErrorCode::Error;
		}

		pivotKeyForParent = m_vtKeys[nMid];

		m_vtKeys.resize(nMid);
		m_vtValues.resize(nMid);

		return ErrorCode::Success;
	}

	inline ErrorCode split(std::shared_ptr<SelfType> ptrSibling, KeyType& pivotKeyForParent)
	{
		size_t nMid = m_vtKeys.size() / 2;

		ptrSibling->m_vtKeys.assign(m_vtKeys.begin() + nMid, m_vtKeys.end());
		ptrSibling->m_vtValues.assign(m_vtValues.begin() + nMid, m_vtValues.end());

		pivotKeyForParent = m_vtKeys[nMid];

		m_vtKeys.resize(nMid);
		m_vtValues.resize(nMid);

		return ErrorCode::Success;
	}

	inline void moveAnEntityFromLHSSibling(std::shared_ptr<SelfType> ptrLHSSibling, KeyType& pivotKeyForParent)
	{
		KeyType key = ptrLHSSibling->m_vtKeys.back();
		ValueType value = ptrLHSSibling->m_vtValues.back();

		ptrLHSSibling->m_vtKeys.pop_back();
		ptrLHSSibling->m_vtValues.pop_back();

		if (ptrLHSSibling->m_vtKeys.size() == 0)
		{
			throw new std::logic_error("should not occur!");
		}

		m_vtKeys.insert(m_vtKeys.begin(), key);
		m_vtValues.insert(m_vtValues.begin(), value);

		pivotKeyForParent = key;
	}

	inline void moveAnEntityFromRHSSibling(std::shared_ptr<SelfType> ptrRHSSibling, KeyType& pivotKeyForParent)
	{
		KeyType key = ptrRHSSibling->m_vtKeys.front();
		ValueType value = ptrRHSSibling->m_vtValues.front();

		ptrRHSSibling->m_vtKeys.erase(ptrRHSSibling->m_vtKeys.begin());
		ptrRHSSibling->m_vtValues.erase(ptrRHSSibling->m_vtValues.begin());

		if (ptrRHSSibling->m_vtKeys.size() == 0)
		{
			throw new std::logic_error("should not occur!");
		}

		m_vtKeys.push_back(key);
		m_vtValues.push_back(value);

		pivotKeyForParent = ptrRHSSibling->m_vtKeys.front();
	}

	inline void mergeNode(std::shared_ptr<SelfType> ptrSibling)
	{
		m_vtKeys.insert(m_vtKeys.end(), ptrSibling->m_vtKeys.begin(), ptrSibling->m_vtKeys.end());
		m_vtValues.insert(m_vtValues.end(), ptrSibling->m_vtValues.begin(), ptrSibling->m_vtValues.end());
	}

public:
	inline size_t getSize() const
	{
		return
			sizeof(uint8_t)
			+ sizeof(size_t)
			+ sizeof(size_t)
			+ (m_vtKeys.size() * sizeof(KeyType))
			+ (m_vtValues.size() * sizeof(ValueType));
	}

	inline void serialize(char*& szBuffer, uint8_t& uidObjectType, size_t& nBufferSize) const
	{
		static_assert(
			std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<ValueType>::value &&
			std::is_standard_layout<ValueType>::value,
			"Can only deserialize POD types with this function");

		uidObjectType = UID;

		size_t nKeyCount = m_vtKeys.size();
		size_t nValueCount = m_vtValues.size();

		nBufferSize = sizeof(uint8_t) + (nKeyCount * sizeof(KeyType)) + (nValueCount * sizeof(ValueType)) + sizeof(size_t) + sizeof(size_t);

		szBuffer = new char[nBufferSize + 1];
		memset(szBuffer, 0, nBufferSize + 1);

		size_t nOffset = 0;
		memcpy(szBuffer, &UID, sizeof(uint8_t));
		nOffset += sizeof(uint8_t);

		memcpy(szBuffer + nOffset, &nKeyCount, sizeof(size_t));
		nOffset += sizeof(size_t);

		memcpy(szBuffer + nOffset, &nValueCount, sizeof(size_t));
		nOffset += sizeof(size_t);

		size_t nKeysSize = nKeyCount * sizeof(KeyType);
		memcpy(szBuffer + nOffset, m_vtKeys.data(), nKeysSize);
		nOffset += nKeysSize;

		size_t nValuesSize = nValueCount * sizeof(ValueType);
		memcpy(szBuffer + nOffset, m_vtValues.data(), nValuesSize);
		nOffset += nValuesSize;

		assert(nBufferSize == nOffset);

		SelfType* _t = new SelfType(szBuffer);
		for (int i = 0; i < _t->m_vtKeys.size(); i++)
		{
			assert(_t->m_vtKeys[i] == m_vtKeys[i]);
		}
		for (int i = 0; i < _t->m_vtValues.size(); i++)
		{
			assert(_t->m_vtValues[i] == m_vtValues[i]);
		}
		delete _t;

		// hint
		/*
		if (std::is_trivial<ObjectUIDType>::value && std::is_standard_layout<ObjectUIDType>::value)
		os.write(reinterpret_cast<const char*>(m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::NodeUID));
		else
		os.write(reinterpret_cast<const char*>(m_vtChildren.data()), nValueCount * sizeof(ObjectUIDType::PODType));

		*/
	}

	inline void writeToStream(std::fstream& os, uint8_t& uidObjectType, size_t& nDataSize) const
	{
		static_assert(
			std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<ValueType>::value &&
			std::is_standard_layout<ValueType>::value,
			"Can only deserialize POD types with this function");

		uidObjectType = SelfType::UID;

		size_t nKeyCount = m_vtKeys.size();
		size_t nValueCount = m_vtValues.size();

		nDataSize = sizeof(uint8_t) + (nKeyCount * sizeof(KeyType)) + (nValueCount * sizeof(ValueType)) + sizeof(size_t) + sizeof(size_t);

		os.write(reinterpret_cast<const char*>(&uidObjectType), sizeof(uint8_t));
		os.write(reinterpret_cast<const char*>(&nKeyCount), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(&nValueCount), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(m_vtKeys.data()), nKeyCount * sizeof(KeyType));
		os.write(reinterpret_cast<const char*>(m_vtValues.data()), nValueCount * sizeof(ValueType));
	}

public:
	void print(std::ofstream& out, size_t nLevel, std::string prefix)
	{
		int nSpace = 7;

		prefix.append(std::string(nSpace - 1, ' '));
		prefix.append("|");

		for (size_t nIndex = 0; nIndex < m_vtKeys.size(); nIndex++)
		{
			out << " " << prefix << std::string(nSpace, '-').c_str() << "(K: " << m_vtKeys[nIndex] << ", V: " << m_vtValues[nIndex] << ")" << std::endl;
		}
	}

	void wieHiestDu() {
		printf("ich heisse DataNode :).\n");
	}
};