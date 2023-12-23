#pragma once
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <optional>

#include <iostream>
#include <fstream>

#include "ErrorCodes.h"

template <typename KeyType, typename ValueType, typename ObjectUIDType, uint8_t TYPE_UID>
class DataNode
{
public:
	static const uint8_t UID = TYPE_UID;

	ObjectUIDType m_keyParent;
private:
	typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID> SelfType;
	typedef std::vector<KeyType>::const_iterator KeyTypeIterator;
	typedef std::vector<ValueType>::const_iterator ValueTypeIterator;

	struct DATANODESTRUCT
	{
		std::vector<KeyType> m_vtKeys;
		std::vector<ValueType> m_vtValues;

/*		DATANODESTRUCT()
		{}

		DATANODESTRUCT(DATANODESTRUCT&& o)
			: m_vtKeys(std::move(o.m_vtKeys))
			, m_vtValues(std::move(o.m_vtValuesk))
		{}

		DATANODESTRUCT(DATANODESTRUCT* o)
			: m_vtKeys(std::move(o->m_vtKeys))
			, m_vtValues(std::move(o->m_vtValuesk))
		{}
*/
	};

private:
	std::shared_ptr<DATANODESTRUCT> m_ptrData;

public:
	~DataNode()
	{
		// TODO: check for ref count?
		m_ptrData.reset();
	}

	DataNode()
		: m_ptrData(make_shared<DATANODESTRUCT>())
	{
	}

	DataNode(char* szData)
		: m_ptrData(make_shared<DATANODESTRUCT>())
	{
		size_t keyCount, valueCount;

		//memcpy(&UID, reinterpret_cast<char*>(szData), sizeof(uint8_t));

		// Deserialize the size of vectors
		uint8_t t;

		memcpy(&t, szData , sizeof(uint8_t));
		memcpy( &keyCount, szData + sizeof(uint8_t), sizeof(size_t));
		memcpy(&valueCount, szData + sizeof(uint8_t) + sizeof(size_t), sizeof(size_t));

		// Resize vectors to accommodate the data
		m_ptrData->m_vtKeys.resize(keyCount);
		m_ptrData->m_vtValues.resize(valueCount);

		// Deserialize vector elements
		memcpy(m_ptrData->m_vtKeys.data(), szData + sizeof(uint8_t) + sizeof(size_t) + sizeof(size_t), keyCount * sizeof(KeyType));
		memcpy(m_ptrData->m_vtValues.data(), szData + sizeof(uint8_t) + sizeof(size_t) + sizeof(size_t) + (keyCount * sizeof(KeyType)), valueCount * sizeof(ValueType));
	}

	DataNode(std::fstream& is)
		: m_ptrData(make_shared<DATANODESTRUCT>())
	{
		size_t keyCount, valueCount;

		is.read(reinterpret_cast<char*>(&keyCount), sizeof(size_t));
		is.read(reinterpret_cast<char*>(&valueCount), sizeof(size_t));

		m_ptrData->m_vtKeys.resize(keyCount);
		m_ptrData->m_vtValues.resize(valueCount);

		is.read(reinterpret_cast<char*>(m_ptrData->m_vtKeys.data()), keyCount * sizeof(KeyType));
		is.read(reinterpret_cast<char*>(m_ptrData->m_vtValues.data()), valueCount * sizeof(ValueType));
	}

	DataNode(KeyTypeIterator itBeginKeys, KeyTypeIterator itEndKeys, ValueTypeIterator itBeginValues, ValueTypeIterator itEndValues)
		: m_ptrData(make_shared<DATANODESTRUCT>())
	{
		m_ptrData->m_vtKeys.assign(itBeginKeys, itEndKeys);
		m_ptrData->m_vtValues.assign(itBeginValues, itEndValues);
	}

	inline void insert(const KeyType& key, const ValueType& value)
	{
		size_t nChildIdx = m_ptrData->m_vtKeys.size();
		for (int nIdx = 0; nIdx < m_ptrData->m_vtKeys.size(); ++nIdx)
		{
			if (key < m_ptrData->m_vtKeys[nIdx])
			{
				nChildIdx = nIdx;
				break;
			}
		}

		m_ptrData->m_vtKeys.insert(m_ptrData->m_vtKeys.begin() + nChildIdx, key);
		m_ptrData->m_vtValues.insert(m_ptrData->m_vtValues.begin() + nChildIdx, value);
	}

	inline ErrorCode remove(const KeyType& key)
	{
		KeyTypeIterator it = std::lower_bound(m_ptrData->m_vtKeys.begin(), m_ptrData->m_vtKeys.end(), key);

		if (it != m_ptrData->m_vtKeys.end() && *it == key)
		{
			int index = it - m_ptrData->m_vtKeys.begin();
			m_ptrData->m_vtKeys.erase(it);
			m_ptrData->m_vtValues.erase(m_ptrData->m_vtValues.begin() + index);

			return ErrorCode::Success;
		}

		return ErrorCode::KeyDoesNotExist;
	}

	inline bool requireSplit(size_t nDegree)
	{
		return m_ptrData->m_vtKeys.size() > nDegree;
	}

	inline bool requireMerge(size_t nDegree)
	{
		return m_ptrData->m_vtKeys.size() <= std::ceil(nDegree / 2.0f);
	}

	inline size_t getKeysCount() {
		return m_ptrData->m_vtKeys.size();
	}

	inline ErrorCode getValue(const KeyType& key, ValueType& value)
	{
		KeyTypeIterator it = std::lower_bound(m_ptrData->m_vtKeys.begin(), m_ptrData->m_vtKeys.end(), key);
		if (it != m_ptrData->m_vtKeys.end() && *it == key)
		{
			size_t index = it - m_ptrData->m_vtKeys.begin();
			value = m_ptrData->m_vtValues[index];

			return ErrorCode::Success;
		}

		return ErrorCode::KeyDoesNotExist;
	}

	template <typename Cache, typename CacheKeyType>
#ifdef __POSITION_AWARE_ITEMS__
	inline ErrorCode split(Cache ptrCache, std::optional<CacheKeyType>& ptrSibling, std::optional<CacheKeyType>& keyParent, KeyType& pivotKey)
#else
	inline ErrorCode split(Cache ptrCache, std::optional<CacheKeyType>& ptrSibling, KeyType& pivotKey)
#endif __POSITION_AWARE_ITEMS__
	{
		size_t nMid = m_ptrData->m_vtKeys.size() / 2;

#ifdef __POSITION_AWARE_ITEMS__
		ptrCache->template createObjectOfType<SelfType>(ptrSibling, keyParent,
			m_ptrData->m_vtKeys.begin() + nMid, m_ptrData->m_vtKeys.end(),
			m_ptrData->m_vtValues.begin() + nMid, m_ptrData->m_vtValues.end());
#else
		ptrCache->template createObjectOfType<SelfType>(ptrSibling,
			m_ptrData->m_vtKeys.begin() + nMid, m_ptrData->m_vtKeys.end(),
			m_ptrData->m_vtValues.begin() + nMid, m_ptrData->m_vtValues.end());
#endif __POSITION_AWARE_ITEMS__

		if (!ptrSibling)
		{
			return ErrorCode::Error;
		}

		pivotKey = m_ptrData->m_vtKeys[nMid];

		m_ptrData->m_vtKeys.resize(nMid);
		m_ptrData->m_vtValues.resize(nMid);

		return ErrorCode::Success;
	}

	inline void moveAnEntityFromLHSSibling(std::shared_ptr<SelfType> ptrLHSSibling, KeyType& pivotKey)
	{
		KeyType key = ptrLHSSibling->m_ptrData->m_vtKeys.back();
		ValueType value = ptrLHSSibling->m_ptrData->m_vtValues.back();

		ptrLHSSibling->m_ptrData->m_vtKeys.pop_back();
		ptrLHSSibling->m_ptrData->m_vtValues.pop_back();

		m_ptrData->m_vtKeys.insert(m_ptrData->m_vtKeys.begin(), key);
		m_ptrData->m_vtValues.insert(m_ptrData->m_vtValues.begin(), value);

		pivotKey = key;
	}

	inline void moveAnEntityFromRHSSibling(std::shared_ptr<SelfType> ptrRHSSibling, KeyType& pivotKey)
	{
		KeyType key = ptrRHSSibling->m_ptrData->m_vtKeys.front();
		ValueType value = ptrRHSSibling->m_ptrData->m_vtValues.front();

		ptrRHSSibling->m_ptrData->m_vtKeys.erase(ptrRHSSibling->m_ptrData->m_vtKeys.begin());
		ptrRHSSibling->m_ptrData->m_vtValues.erase(ptrRHSSibling->m_ptrData->m_vtValues.begin());

		m_ptrData->m_vtKeys.push_back(key);
		m_ptrData->m_vtValues.push_back(value);

		pivotKey = ptrRHSSibling->m_ptrData->m_vtKeys.front();
	}

	inline void mergeNode(std::shared_ptr<SelfType> ptrSibling)
	{
		m_ptrData->m_vtKeys.insert(m_ptrData->m_vtKeys.end(), ptrSibling->m_ptrData->m_vtKeys.begin(), ptrSibling->m_ptrData->m_vtKeys.end());
		m_ptrData->m_vtValues.insert(m_ptrData->m_vtValues.end(), ptrSibling->m_ptrData->m_vtValues.begin(), ptrSibling->m_ptrData->m_vtValues.end());
	}

public:
	inline const char* getSerializedBytes(uint8_t& uidObjectType, size_t& nDataSize)
	{
		static_assert(
			std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<ValueType>::value &&
			std::is_standard_layout<ValueType>::value,
			"Can only deserialize POD types with this function");

		size_t keyCount = m_ptrData->m_vtKeys.size();
		size_t valueCount = m_ptrData->m_vtValues.size();

		nDataSize = sizeof(uint8_t) + (keyCount * sizeof(KeyType)) + (valueCount * sizeof(ValueType)) + sizeof(size_t) + sizeof(size_t);
		char* szBuffer = new char[nDataSize];
		memset(szBuffer, 0, nDataSize);

		size_t offset = 0;
		memcpy(szBuffer, &UID, sizeof(uint8_t));

		offset += sizeof(uint8_t);
		memcpy(szBuffer + offset, &keyCount, sizeof(size_t));

		size_t t = 0;
		memcpy(&t, szBuffer + offset, sizeof(size_t));

		offset += sizeof(size_t);
		memcpy(szBuffer + offset, &valueCount, sizeof(size_t));

		t = 0;
		memcpy(&t, szBuffer + offset, sizeof(size_t));

		offset += sizeof(size_t);
		memcpy(szBuffer + offset, m_ptrData->m_vtKeys.data(), keyCount * sizeof(KeyType));

		//std::vector<KeyType> m_vt;
		//memcpy(szBuffer + offset, m_ptrData->m_vtKeys.data(), keyCount * sizeof(KeyType));


		offset += (keyCount * sizeof(KeyType));
		memcpy(szBuffer + offset, m_ptrData->m_vtValues.data(), valueCount * sizeof(ValueType));

		std::shared_ptr<SelfType> _p = std::make_shared<SelfType>(szBuffer);

		nDataSize = sizeof(DATANODESTRUCT);
		uidObjectType = UID;

		return reinterpret_cast<const char*>(m_ptrData.get());
	}

	inline void writeToStream(std::fstream& os, uint8_t& uidObjectType, size_t& nDataSize)
	{
		static_assert(
			std::is_trivial<KeyType>::value &&
			std::is_standard_layout<KeyType>::value &&
			std::is_trivial<ValueType>::value &&
			std::is_standard_layout<ValueType>::value,
			"Can only deserialize POD types with this function");

		uidObjectType = UID;

		size_t keyCount = m_ptrData->m_vtKeys.size();
		size_t valueCount = m_ptrData->m_vtValues.size();

		nDataSize = sizeof(uint8_t) + (keyCount * sizeof(KeyType)) + (valueCount * sizeof(ValueType)) + sizeof(size_t) + sizeof(size_t);

		os.write(reinterpret_cast<const char*>(&UID), sizeof(uint8_t));
		os.write(reinterpret_cast<const char*>(&keyCount), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(&valueCount), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtKeys.data()), keyCount * sizeof(KeyType));
		os.write(reinterpret_cast<const char*>(m_ptrData->m_vtValues.data()), valueCount * sizeof(ValueType));
	}

	void instantiateSelf(std::byte* bytes)
	{
		m_ptrData.reset(reinterpret_cast<DATANODESTRUCT*>(bytes));
	}

public:
	void printkeys()
	{
		std::cout << "[D](";
		for (size_t nIndex = 0; nIndex < m_ptrData->m_vtKeys.size(); nIndex++)
		{
			std::cout << m_ptrData->m_vtKeys[nIndex];
			if(nIndex < m_ptrData->m_vtKeys.size() - 1)
				std::cout << ",";
		}
		std::cout << ")";
	}

	void print(size_t nLevel)
	{
		std::cout << std::string(nLevel, '.').c_str() << " **LEVEL**:[" << nLevel << "] BEGIN" << std::endl;

		for (size_t nIndex = 0; nIndex < m_ptrData->m_vtKeys.size(); nIndex++)
		{
			std::cout << std::string(nLevel, '.').c_str() << " ==> key: " << m_ptrData->m_vtKeys[nIndex] << ", value: " << m_ptrData->m_vtValues[nIndex] << std::endl;
		}

		std::cout << std::string(nLevel, '.').c_str() << " **LEVEL**:[" << nLevel << "] END" << std::endl;
	}

	void wieHiestDu() {
		printf("ich heisse DataNode :).\n");
	}
};