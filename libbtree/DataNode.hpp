#pragma once
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <optional>

#include "ErrorCodes.h"

template <typename KeyType, typename ValueType, uint8_t TYPE_UID>
class DataNode
{
public:
	static const uint8_t UID = TYPE_UID;

private:
	typedef DataNode<KeyType, ValueType, TYPE_UID> SelfType;
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

	DataNode(DATANODESTRUCT* bytes)
	{
		//m_ptrData(ptrData);
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
	inline ErrorCode split(Cache ptrCache, std::optional<CacheKeyType>& ptrSibling, KeyType& pivotKey)
	{
		size_t nMid = m_ptrData->m_vtKeys.size() / 2;

		ptrSibling = ptrCache->template createObjectOfType<SelfType>(
			m_ptrData->m_vtKeys.begin() + nMid, m_ptrData->m_vtKeys.end(),
			m_ptrData->m_vtValues.begin() + nMid, m_ptrData->m_vtValues.end());

		if (!ptrSibling)
		{
			return ErrorCode::Error;
		}

		pivotKey = m_ptrData->m_vtKeys[nMid];

		m_ptrData->m_vtKeys.resize(nMid);
		m_ptrData->m_vtValues.resize(nMid);

		return ErrorCode::Success;
	}

	inline void moveAnEntityFromLHSSibling(shared_ptr<SelfType> ptrLHSSibling, KeyType& pivotKey)
	{
		KeyType key = ptrLHSSibling->m_ptrData->m_vtKeys.back();
		ValueType value = ptrLHSSibling->m_ptrData->m_vtValues.back();

		ptrLHSSibling->m_ptrData->m_vtKeys.pop_back();
		ptrLHSSibling->m_ptrData->m_vtValues.pop_back();

		m_ptrData->m_vtKeys.insert(m_ptrData->m_vtKeys.begin(), key);
		m_ptrData->m_vtValues.insert(m_ptrData->m_vtValues.begin(), value);

		pivotKey = key;
	}

	inline void moveAnEntityFromRHSSibling(shared_ptr<SelfType> ptrRHSSibling, KeyType& pivotKey)
	{
		KeyType key = ptrRHSSibling->m_ptrData->m_vtKeys.front();
		ValueType value = ptrRHSSibling->m_ptrData->m_vtValues.front();

		ptrRHSSibling->m_ptrData->m_vtKeys.erase(ptrRHSSibling->m_ptrData->m_vtKeys.begin());
		ptrRHSSibling->m_ptrData->m_vtValues.erase(ptrRHSSibling->m_ptrData->m_vtValues.begin());

		m_ptrData->m_vtKeys.push_back(key);
		m_ptrData->m_vtValues.push_back(value);

		pivotKey = ptrRHSSibling->m_ptrData->m_vtKeys.front();
	}

	inline void mergeNode(shared_ptr<SelfType> ptrSibling)
	{
		m_ptrData->m_vtKeys.insert(m_ptrData->m_vtKeys.end(), ptrSibling->m_ptrData->m_vtKeys.begin(), ptrSibling->m_ptrData->m_vtKeys.end());
		m_ptrData->m_vtValues.insert(m_ptrData->m_vtValues.end(), ptrSibling->m_ptrData->m_vtValues.begin(), ptrSibling->m_ptrData->m_vtValues.end());
	}

public:
	std::tuple<uint8_t, const std::byte*, size_t> getSerializedBytes()
	{
		const std::byte* bytes = reinterpret_cast<const std::byte*>(m_ptrData.get());

		//std::byte* _bytes = reinterpret_cast<std::byte*>(m_ptrData.get());
		//DATANODESTRUCT* o = reinterpret_cast<DATANODESTRUCT*>(_bytes);

		return std::tuple<uint8_t, const std::byte*, size_t>(UID, bytes, sizeof(DATANODESTRUCT));

	}

	void instantiateSelf(std::byte* bytes)
	{
		m_ptrData.reset(reinterpret_cast<DATANODESTRUCT*>(bytes));
	}

public:
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