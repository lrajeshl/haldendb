#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <cstring>
#include <stdexcept>

//#pragma pack(1)

class ObjectFatUID
{
public:
	enum Media
	{
		None = 0,
		Volatile,
		DRAM,
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
		uint8_t m_nType;
		uint8_t m_nMediaType;
		union
		{
			uintptr_t m_ptrVolatile;
			FilePointer m_ptrFile;
		} FATPOINTER;
	};

public:
	NodeUID m_uid;
	size_t m_hash;

public:
	inline uint8_t getObjectType() const
	{
		return m_uid.m_nType;
	}

	inline uint8_t getMediaType() const
	{
		return m_uid.m_nMediaType;
	}

	inline uintptr_t getVolatilePointerValue() const
	{
		return m_uid.FATPOINTER.m_ptrVolatile;
	}

	inline uint32_t getPersistentPointerValue() const
	{
		return m_uid.FATPOINTER.m_ptrFile.m_nOffset;
	}

	inline uint32_t getPersistentBytesSize() const
	{
		return m_uid.FATPOINTER.m_ptrFile.m_nSize;
	}

	template <typename... Args>
	static ObjectFatUID createAddressFromArgs(Media nMediaType, Args... args)
	{
		switch (nMediaType)
		{
		case None:
			throw new std::logic_error("should not occur!");
			break;
		case Volatile:
			return createAddressFromVolatilePointer(args...);
			break;
		case DRAM:
			return createAddressFromDRAMCacheCounter(args...);
			break;
		case PMem:
			break;
		case File:
			return createAddressFromFileOffset(args...);
			break;
		}

		throw new std::logic_error("should not occur!");
	}

	static ObjectFatUID createAddressFromFileOffset(uint8_t nType, uint32_t nPos, uint16_t nBlockSize, uint32_t nSize)
	{
		ObjectFatUID key;
		key.m_uid.m_nType = nType;
		key.m_uid.m_nMediaType = File;
		key.m_uid.FATPOINTER.m_ptrFile.m_nOffset = nPos * nBlockSize;
		key.m_uid.FATPOINTER.m_ptrFile.m_nSize = nSize;

		return key;
	}

	static ObjectFatUID createAddressFromVolatilePointer(uint8_t nType, uintptr_t ptr, ...)
	{
		ObjectFatUID key;
		key.m_uid.m_nType = nType;
		key.m_uid.m_nMediaType = Volatile;
		key.m_uid.FATPOINTER.m_ptrVolatile = ptr;

		return key;
	}

	static ObjectFatUID createAddressFromDRAMCacheCounter(uint8_t nType, uint32_t nPos, uint16_t nBlockSize, uint32_t nSize)
	{
		ObjectFatUID key;
		key.m_uid.m_nType = nType;
		key.m_uid.m_nMediaType = DRAM;
		key.m_uid.FATPOINTER.m_ptrFile.m_nOffset = nPos * nBlockSize;
		key.m_uid.FATPOINTER.m_ptrFile.m_nSize = nSize;

		return key;
	}

	bool operator==(const ObjectFatUID& rhs) const
	{
		if (m_uid.m_nType != rhs.m_uid.m_nType
			|| m_uid.m_nMediaType != rhs.m_uid.m_nMediaType)
		{
			return false;
		}

		switch (m_uid.m_nMediaType)
		{
		case Volatile:
			return m_uid.FATPOINTER.m_ptrVolatile == rhs.m_uid.FATPOINTER.m_ptrVolatile;
		case DRAM:
			return m_uid.FATPOINTER.m_ptrVolatile == rhs.m_uid.FATPOINTER.m_ptrVolatile;
		case PMem:
			return false;
		case File:
			return m_uid.FATPOINTER.m_ptrFile.m_nOffset == rhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset &&
				m_uid.FATPOINTER.m_ptrFile.m_nSize == rhs.m_uid.FATPOINTER.m_ptrFile.m_nSize;
		default:
			return std::memcmp(this, &rhs.m_uid, sizeof(NodeUID)) == 0;
		}

		//Following is comiler specific!
		//return memcmp(&m_uid, &rhs.m_uid, sizeof(NodeUID)) == 0;
	}

	bool operator <(const ObjectFatUID& rhs) const
	{
		if (m_uid.m_nType < rhs.m_uid.m_nType)
			return true;
		else if (m_uid.m_nType > rhs.m_uid.m_nType)
			return false;

		if (m_uid.m_nMediaType < rhs.m_uid.m_nMediaType)
			return true;
		else if (m_uid.m_nMediaType > rhs.m_uid.m_nMediaType)
			return false;

		switch (m_uid.m_nMediaType)
		{
		case Volatile:
			return m_uid.FATPOINTER.m_ptrVolatile < rhs.m_uid.FATPOINTER.m_ptrVolatile;

		case DRAM:
			return m_uid.FATPOINTER.m_ptrVolatile < rhs.m_uid.FATPOINTER.m_ptrVolatile;

		case File:
			if (m_uid.FATPOINTER.m_ptrFile.m_nOffset < rhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset)
				return true;
			else if (m_uid.FATPOINTER.m_ptrFile.m_nOffset > rhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset)
				return false;

			return m_uid.FATPOINTER.m_ptrFile.m_nSize < rhs.m_uid.FATPOINTER.m_ptrFile.m_nSize;

		default:
			return std::memcmp(this, &rhs, sizeof(NodeUID)) < 0;
		}

		//Following is comiler specific!
		//return memcmp(&m_uid, &rhs.m_uid, sizeof(NodeUID)) < 0;
	}

	inline size_t gethash() const
	{
		return m_hash;
	}

	void recalculateHash()
	{
		m_hash = std::hash<uint8_t>()(m_uid.m_nType);
		m_hash ^= std::hash<uint8_t>()(m_uid.m_nMediaType);

		switch (m_uid.m_nMediaType)
		{
		case ObjectFatUID::Media::None:
			break;
		case ObjectFatUID::Media::Volatile:
			m_hash ^= std::hash<uintptr_t>()(m_uid.FATPOINTER.m_ptrVolatile);
			break;
		case ObjectFatUID::Media::DRAM:
			m_hash ^= std::hash<uintptr_t>()(m_uid.FATPOINTER.m_ptrVolatile);
			break;
		case ObjectFatUID::Media::File:
			size_t offsetHash = std::hash<uint32_t>()(m_uid.FATPOINTER.m_ptrFile.m_nOffset);
			size_t sizeHash = std::hash<uint32_t>()(m_uid.FATPOINTER.m_ptrFile.m_nSize);
			m_hash ^= offsetHash ^ (sizeHash + 0x9e3779b9 + (offsetHash << 6) + (offsetHash >> 2));
			break;
		}
	}

	bool compare(const ObjectFatUID& rhs) const
	{
		return *this == rhs;
	}

	struct HashFunction
	{
	public:
		size_t operator()(const ObjectFatUID& rhs) const
		{
			return std::hash<uint8_t>()(rhs.m_uid.m_nType)
				^ std::hash<uint8_t>()(rhs.m_uid.m_nMediaType)
				^ std::hash<uintptr_t>()(rhs.m_uid.FATPOINTER.m_ptrVolatile)
				^ std::hash<uint32_t>()(rhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset)
				^ std::hash<uint32_t>()(rhs.m_uid.FATPOINTER.m_ptrFile.m_nSize);
		}
	};

	struct EqualFunction
	{
	public:
		bool operator()(const ObjectFatUID& lhs, const ObjectFatUID& rhs) const
		{
			if (lhs.m_uid.m_nType < rhs.m_uid.m_nType)
				return true;
			else if (lhs.m_uid.m_nType > rhs.m_uid.m_nType)
				return false;

			if (lhs.m_uid.m_nMediaType < rhs.m_uid.m_nMediaType)
				return true;
			else if (lhs.m_uid.m_nMediaType > rhs.m_uid.m_nMediaType)
				return false;

			switch (lhs.m_uid.m_nMediaType) {
			case Volatile:
				return lhs.m_uid.FATPOINTER.m_ptrVolatile < rhs.m_uid.FATPOINTER.m_ptrVolatile;

			case DRAM:
				return lhs.m_uid.FATPOINTER.m_ptrVolatile < rhs.m_uid.FATPOINTER.m_ptrVolatile;

			case File:
				if (lhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset < rhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset)
					return true;
				else if (lhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset > rhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset)
					return false;

				return lhs.m_uid.FATPOINTER.m_ptrFile.m_nSize < rhs.m_uid.FATPOINTER.m_ptrFile.m_nSize;

			default:
				return std::memcmp(&lhs, &rhs, sizeof(NodeUID)) < 0;
			}

			//Following is comiler specific!
			//return memcmp(&lhs.m_uid, &rhs.m_uid, sizeof(NodeUID)) == 0;
		}
	};


public:
	~ObjectFatUID()
	{
		m_uid.m_nType = 0;
		m_uid.m_nMediaType = 0;
		m_uid.FATPOINTER.m_ptrVolatile = 0;
		m_uid.FATPOINTER.m_ptrFile.m_nOffset = 0;
		m_uid.FATPOINTER.m_ptrFile.m_nSize = 0;

		m_hash = 0;
	}

	ObjectFatUID(const char* uid)
	{
		const NodeUID* i = reinterpret_cast<const NodeUID*>(uid);
		m_uid = *i;
		m_hash = 0;
	}

	ObjectFatUID()
	{
		m_uid.m_nType = 0;
		m_uid.m_nMediaType = 0;
		m_uid.FATPOINTER.m_ptrVolatile = 0;
		m_uid.FATPOINTER.m_ptrFile.m_nOffset = 0;
		m_uid.FATPOINTER.m_ptrFile.m_nSize = 0;

		m_hash = 0;
	}

	ObjectFatUID(const ObjectFatUID& other)
	{
		m_uid.m_nType = other.m_uid.m_nType;
		m_uid.m_nMediaType = other.m_uid.m_nMediaType;
		m_uid.FATPOINTER.m_ptrVolatile = other.m_uid.FATPOINTER.m_ptrVolatile;
		m_uid.FATPOINTER.m_ptrFile.m_nOffset = other.m_uid.FATPOINTER.m_ptrFile.m_nOffset;
		m_uid.FATPOINTER.m_ptrFile.m_nSize = other.m_uid.FATPOINTER.m_ptrFile.m_nSize;

		recalculateHash();
	}

	ObjectFatUID(ObjectFatUID& other)
	{
		m_uid.m_nType = other.m_uid.m_nType;
		m_uid.m_nMediaType = other.m_uid.m_nMediaType;
		m_uid.FATPOINTER.m_ptrVolatile = other.m_uid.FATPOINTER.m_ptrVolatile;
		m_uid.FATPOINTER.m_ptrFile.m_nOffset = other.m_uid.FATPOINTER.m_ptrFile.m_nOffset;
		m_uid.FATPOINTER.m_ptrFile.m_nSize = other.m_uid.FATPOINTER.m_ptrFile.m_nSize;

		recalculateHash();
	}

	std::string toString() const
	{
		std::string szData;
		switch (m_uid.m_nMediaType)
		{
		case None:
			break;
		case Volatile:
			szData.append("V:");
			szData.append(std::to_string(m_uid.FATPOINTER.m_ptrVolatile));
			break;
		case DRAM:
			szData.append("D:");
			szData.append(std::to_string(m_uid.FATPOINTER.m_ptrVolatile));
			break;
		case PMem:
			szData.append("P:");
			break;
		case File:
			szData.append("F:");
			szData.append(std::to_string(m_uid.FATPOINTER.m_ptrFile.m_nOffset));
			szData.append(":");
			szData.append(std::to_string(m_uid.FATPOINTER.m_ptrFile.m_nSize));
			break;
		}
		return szData;
	}
};

namespace std {
	template <>
	struct hash<ObjectFatUID> {
		size_t operator()(const ObjectFatUID& rhs) const
		{
			return rhs.gethash();
		}
	};
}