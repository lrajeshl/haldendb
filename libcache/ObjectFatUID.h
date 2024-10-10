#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <cstring>
#include <stdexcept>
#include <optional>

class ObjectFatUID
{
public:
	enum StorageMedia
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

private:
	NodeUID m_uid;
	//size_t m_hash;

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

	inline uint32_t getPersistentObjectSize() const
	{
		return m_uid.FATPOINTER.m_ptrFile.m_nSize;
	}

public:
	template <typename... Args>
	static ObjectFatUID createAddressFromArgs(StorageMedia nMediaType, Args... args)
	{
		switch (nMediaType)
		{
		case None:
			throw new std::logic_error(".....");
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

		throw new std::logic_error(".....");
	}

	static ObjectFatUID createAddressFromFileOffset(uint8_t nType, uint32_t nOffset, uint32_t nSize)
	{
		ObjectFatUID uid;
		uid.m_uid.m_nType = nType;
		uid.m_uid.m_nMediaType = File;
		uid.m_uid.FATPOINTER.m_ptrFile.m_nOffset = nOffset;
		uid.m_uid.FATPOINTER.m_ptrFile.m_nSize = nSize;

		return std::move(uid);
	}

	static ObjectFatUID createAddressFromVolatilePointer(uint8_t nType, uintptr_t ptr, ...)
	{
		ObjectFatUID uid;
		uid.m_uid.m_nType = nType;
		uid.m_uid.m_nMediaType = Volatile;
		uid.m_uid.FATPOINTER.m_ptrVolatile = ptr;

		return std::move(uid);
	}

	static ObjectFatUID createAddressFromDRAMCacheCounter(uint8_t nType, uint32_t nOffset, uint32_t nSize)
	{
		ObjectFatUID uid;
		uid.m_uid.m_nType = nType;
		uid.m_uid.m_nMediaType = DRAM;
		uid.m_uid.FATPOINTER.m_ptrFile.m_nOffset = nOffset;
		uid.m_uid.FATPOINTER.m_ptrFile.m_nSize = nSize;

		return std::move(uid);
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

	size_t gethash() const
	{
		size_t hashValue = 0;
		hashValue ^= std::hash<uint8_t>()(m_uid.m_nType);
		hashValue ^= std::hash<uint8_t>()(m_uid.m_nMediaType);

		switch (m_uid.m_nMediaType)
		{
		case ObjectFatUID::StorageMedia::None:
			break;
		case ObjectFatUID::StorageMedia::Volatile:
			hashValue ^= std::hash<uintptr_t>()(m_uid.FATPOINTER.m_ptrVolatile);
			break;
		case ObjectFatUID::StorageMedia::DRAM:
			hashValue ^= std::hash<uintptr_t>()(m_uid.FATPOINTER.m_ptrVolatile);
			break;
		case ObjectFatUID::StorageMedia::File:
			size_t offsetHash = std::hash<uint32_t>()(m_uid.FATPOINTER.m_ptrFile.m_nOffset);
			size_t sizeHash = std::hash<uint32_t>()(m_uid.FATPOINTER.m_ptrFile.m_nSize);
			hashValue ^= offsetHash ^ (sizeHash + 0x9e3779b9 + (offsetHash << 6) + (offsetHash >> 2));
			break;
		}

		return hashValue;
	}

	bool compare(const ObjectFatUID& rhs) const
	{
		return *this == rhs;
	}

/*
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
*/

public:
	~ObjectFatUID()
	{
		m_uid.m_nType = 0;
		m_uid.m_nMediaType = 0;
		m_uid.FATPOINTER.m_ptrVolatile = 0;
		m_uid.FATPOINTER.m_ptrFile.m_nOffset = 0;
		m_uid.FATPOINTER.m_ptrFile.m_nSize = 0;

		//m_hash = 0;
	}

	ObjectFatUID(const char* uid)
	{
		const NodeUID* i = reinterpret_cast<const NodeUID*>(uid);
		m_uid = *i;
		//m_hash = 0;
	}

	ObjectFatUID()
	{
		m_uid.m_nType = 0;
		m_uid.m_nMediaType = 0;
		m_uid.FATPOINTER.m_ptrVolatile = 0;
		m_uid.FATPOINTER.m_ptrFile.m_nOffset = 0;
		m_uid.FATPOINTER.m_ptrFile.m_nSize = 0;

		//m_hash = 0;
	}

	ObjectFatUID(const ObjectFatUID& other)
	{
		m_uid.m_nType = other.m_uid.m_nType;
		m_uid.m_nMediaType = other.m_uid.m_nMediaType;
		m_uid.FATPOINTER.m_ptrVolatile = other.m_uid.FATPOINTER.m_ptrVolatile;
		m_uid.FATPOINTER.m_ptrFile.m_nOffset = other.m_uid.FATPOINTER.m_ptrFile.m_nOffset;
		m_uid.FATPOINTER.m_ptrFile.m_nSize = other.m_uid.FATPOINTER.m_ptrFile.m_nSize;

		//m_hash = 0;
	}

	ObjectFatUID& operator=(const ObjectFatUID& other)
	{
		m_uid = other.m_uid;
		return *this;
	}

	ObjectFatUID(ObjectFatUID&& other) noexcept
	{
		m_uid = std::move(other.m_uid);
	}

	ObjectFatUID& operator=(ObjectFatUID&& other) noexcept {
		
		if (this == &other) 
			return *this;
		
		m_uid = std::move(other.m_uid);

		return *this;
	}


	//ObjectFatUID(std::optional<ObjectFatUID>& other) noexcept
	//{
	//	m_uid.m_nType = other.value().m_uid.m_nType;
	//	m_uid.m_nMediaType = other.value().m_uid.m_nMediaType;
	//	m_uid.FATPOINTER.m_ptrVolatile = other.value().m_uid.FATPOINTER.m_ptrVolatile;
	//	m_uid.FATPOINTER.m_ptrFile.m_nOffset = other.value().m_uid.FATPOINTER.m_ptrFile.m_nOffset;
	//	m_uid.FATPOINTER.m_ptrFile.m_nSize = other.value().m_uid.FATPOINTER.m_ptrFile.m_nSize;

	//	//m_hash = 0;
	//}

	//ObjectFatUID(const ObjectFatUID&) = delete;
	//ObjectFatUID& operator=(const ObjectFatUID&) = delete;

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

	template <>
	struct hash<const ObjectFatUID*> {
		size_t operator()(const ObjectFatUID* rhs) const
		{
			return rhs->gethash();
		}
	};

	template <>
	struct hash<const std::shared_ptr<ObjectFatUID>> {
		size_t operator()(const std::shared_ptr<ObjectFatUID> rhs) const
		{
			return rhs->gethash();
		}
	};

	template <>
	struct equal_to<ObjectFatUID> {
		bool operator()(const ObjectFatUID& lhs, const ObjectFatUID& rhs) const
		{
			return lhs.compare(rhs);
		}
	};

	template <>
	struct equal_to<const ObjectFatUID*> {
		bool operator()(const ObjectFatUID* lhs, const ObjectFatUID* rhs) const
		{
			return lhs->compare(*rhs);
		}
	};

	template <>
	struct equal_to<const std::shared_ptr<ObjectFatUID>> {
		bool operator()(const std::shared_ptr<ObjectFatUID> lhs, const std::shared_ptr<ObjectFatUID> rhs) const
		{
			return lhs->compare(*rhs);
		}
	};
}