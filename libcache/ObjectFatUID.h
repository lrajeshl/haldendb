#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <optional>
#include <vector>

class ObjectFatUID
{
public:
	enum Media
	{
		None = 0,
		Volatile,
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
		} FATPOINTER;
	};

	NodeUID m_uid;

	static ObjectFatUID createAddressFromFileOffset(uint32_t nPos, uint32_t nSize)
	{
		ObjectFatUID key;
		key.m_uid.m_nMediaType = File;
		key.m_uid.FATPOINTER.m_ptrFile.m_nOffset = nPos;
		key.m_uid.FATPOINTER.m_ptrFile.m_nSize= nSize;

		return key;
	}

	static ObjectFatUID createAddressFromVolatilePointer(uintptr_t ptr)
	{
		ObjectFatUID key;
		key.m_uid.m_nMediaType = Volatile;
		key.m_uid.FATPOINTER.m_ptrVolatile = ptr;

		return key;
	}

	bool operator==(const ObjectFatUID& rhs) const 
	{
		return memcmp(&m_uid, &rhs.m_uid, sizeof(NodeUID)) == 0;
	}

	bool operator <(const ObjectFatUID& rhs) const
	{
		return memcmp(&m_uid, &rhs.m_uid, sizeof(NodeUID)) < 1;
	}

	struct HashFunction
	{
	public:
		size_t operator()(const ObjectFatUID& rhs) const
		{
			return std::hash<uint8_t>()(rhs.m_uid.m_nMediaType)
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
			return memcmp(&lhs.m_uid, &rhs.m_uid, sizeof(NodeUID)) == 0;
		}
	};


public:
	ObjectFatUID()
	{
	}

	std::string toString()
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
			size_t hashValue = 0;
			hashValue ^= std::hash<uint8_t>()(rhs.m_uid.m_nMediaType);

			switch (rhs.m_uid.m_nMediaType)
			{
			case ObjectFatUID::Media::Volatile:
				hashValue ^= std::hash<uintptr_t>()(rhs.m_uid.FATPOINTER.m_ptrVolatile);
				break;
			case ObjectFatUID::Media::File:
				size_t offsetHash = std::hash<uint32_t>()(rhs.m_uid.FATPOINTER.m_ptrFile.m_nOffset);
				size_t sizeHash = std::hash<uint32_t>()(rhs.m_uid.FATPOINTER.m_ptrFile.m_nSize);
				hashValue ^= offsetHash ^ (sizeHash + 0x9e3779b9 + (offsetHash << 6) + (offsetHash >> 2));
				break;
			}

			return hashValue;
		}
	};
}


struct UIDUpdate {
	std::optional<ObjectFatUID> uidUpdated;
	std::optional<ObjectFatUID> uidParent;
	std::vector<std::pair<ObjectFatUID, std::optional<ObjectFatUID>>> vtChildUpdates;

};

struct UIDUpdateRequest
{
public:
	ObjectFatUID uidObject;
	std::optional<ObjectFatUID> uidParent;
	std::optional<ObjectFatUID> uidObject_Updated;

	UIDUpdateRequest()
		: uidParent(std::nullopt)
		, uidObject_Updated(std::nullopt)
	{
	}

	UIDUpdateRequest(const ObjectFatUID& uidObject, const std::optional<ObjectFatUID>& uidParent)
		: uidObject_Updated(std::nullopt)
	{
		this->uidObject = uidObject;
		this->uidParent = uidParent;
	}
};

template< typename ObjectType>
struct ObjectFlushRequest
{
public:
	UIDUpdateRequest uidDetails;
	std::shared_ptr<ObjectType> ptrObject;

	ObjectFlushRequest(const ObjectFatUID& uidObject, const std::optional<ObjectFatUID>& uidParent, std::shared_ptr<ObjectType> ptrObject)
	{
		uidDetails.uidObject = uidObject;
		uidDetails.uidParent = uidParent;
		this->ptrObject = ptrObject;
	}
};


