#pragma once
#include <cstdint>
#include <memory>

class CacheObjectFatKey
{
public:
	enum Media
	{
		Volatile = 0,
		PMem,
		File
	};

	struct FilePointer
	{
		uint32_t m_nOffset;
		uint32_t m_nSize;
	};

	struct NodeUID
	{
		uint8_t m_nMediaType;
		union
		{
			uintptr_t m_ptrVolatile;
			FilePointer m_ptrFile;
		};
	};

	NodeUID m_uid;

	static CacheObjectFatKey createAddressFromFileOffset(uint32_t nPos, uint32_t nSize)
	{
		CacheObjectFatKey key;
		key.m_uid.m_nMediaType = File;
		key.m_uid.m_ptrFile.m_nOffset = nPos;
		key.m_uid.m_ptrFile.m_nSize= nSize;

		return key;
	}

	static CacheObjectFatKey createAddressFromVolatilePointer(uintptr_t ptr)
	{
		CacheObjectFatKey key;
		key.m_uid.m_nMediaType = Volatile;
		key.m_uid.m_ptrVolatile = ptr;

		return key;
	}

	bool operator==(const CacheObjectFatKey& rhs) const {
		return (m_uid.m_nMediaType == rhs.m_uid.m_nMediaType)
			&& (m_uid.m_ptrFile.m_nOffset == rhs.m_uid.m_ptrFile.m_nOffset)
			&& (m_uid.m_ptrFile.m_nSize == rhs.m_uid.m_ptrFile.m_nSize)
			&& (m_uid.m_ptrVolatile == rhs.m_uid.m_ptrVolatile);
	}

	bool operator <(const CacheObjectFatKey& rhs) const
	{
		return (m_uid.m_nMediaType < rhs.m_uid.m_nMediaType)
			&& (m_uid.m_ptrFile.m_nOffset < rhs.m_uid.m_ptrFile.m_nOffset)
			&& (m_uid.m_ptrFile.m_nSize < rhs.m_uid.m_ptrFile.m_nSize)
			&& (m_uid.m_ptrVolatile < rhs.m_uid.m_ptrVolatile);
	}

	struct HashFunction
	{
	public:
		size_t operator()(const CacheObjectFatKey& rhs) const
		{
			return std::hash<uint8_t>()(rhs.m_uid.m_nMediaType)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrFile.m_nOffset)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrFile.m_nSize)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrVolatile);
		}
	};

	struct EqualFunction
	{
	public:
		bool operator()(const CacheObjectFatKey& lhs, const CacheObjectFatKey& rhs) const {
			return (lhs.m_uid.m_nMediaType == rhs.m_uid.m_nMediaType)
				&& (lhs.m_uid.m_ptrFile.m_nOffset == rhs.m_uid.m_ptrFile.m_nOffset)
				&& (lhs.m_uid.m_ptrFile.m_nSize == rhs.m_uid.m_ptrFile.m_nSize)
				&& (lhs.m_uid.m_ptrVolatile == rhs.m_uid.m_ptrVolatile);
		}
	};


public:
	CacheObjectFatKey()
	{
	}
};

namespace std {
	template <>
	struct hash<CacheObjectFatKey> {
		size_t operator()(const CacheObjectFatKey& rhs) const
		{
			return std::hash<uint8_t>()(rhs.m_uid.m_nMediaType)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrFile.m_nOffset)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrFile.m_nSize)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrVolatile);
		}
	};
}

