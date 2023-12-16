#pragma once
#include <cstdint>
#include <memory>

class CacheObjectKey
{
public:
	uintptr_t m_ptrVolatile;

	static CacheObjectKey createAddressFromVolatilePointer(uintptr_t ptr)
	{
		CacheObjectKey key;
		key.m_ptrVolatile = ptr;

		return key;
	}

	bool operator==(const CacheObjectKey& rhs) const {
		return m_ptrVolatile == rhs.m_ptrVolatile;
	}

	bool operator <(const CacheObjectKey& rhs) const
	{
		return m_ptrVolatile < rhs.m_ptrVolatile;
	}

	struct HashFunction
	{
	public:
		size_t operator()(const CacheObjectKey& rhs) const
		{
			return std::hash<uint32_t>()(rhs.m_ptrVolatile);
		}
	};

	struct EqualFunction
	{
	public:
		bool operator()(const CacheObjectKey& lhs, const CacheObjectKey& rhs) const {
			return lhs.m_ptrVolatile == rhs.m_ptrVolatile;
		}
	};


public:
	CacheObjectKey()
	{
	}
};

namespace std {
	template <>
	struct hash<CacheObjectKey> {
		size_t operator()(const CacheObjectKey& rhs) const
		{
			return std::hash<uint32_t>()(rhs.m_ptrVolatile);
		}
	};
}