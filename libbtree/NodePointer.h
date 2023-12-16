#pragma once
#include <cstdint>
#include <memory>

class NodePointer
{
public:
	enum Media
	{
		Volatile = 0,
		PMem,
		File
	};

	struct NodeUID
	{
		uint8_t m_nMediaType;
		union
		{
			uintptr_t m_ptrVolatile;
			uint32_t m_ptrPersistent;
		};
	};

	NodeUID m_uid;

	static NodePointer createAddressFromFileOffset(uint32_t nPos)
	{
		NodePointer key;
		key.m_uid.m_nMediaType = File;
		key.m_uid.m_ptrPersistent = nPos;

		return key;
	}

	static NodePointer createAddressFromVolatilePointer(uintptr_t nPtr)
	{
		NodePointer key;
		key.m_uid.m_nMediaType = Volatile;
		key.m_uid.m_ptrPersistent = nPtr;

		return key;
	}

	bool operator==(const NodePointer& rhs) const {
		return (m_uid.m_nMediaType == rhs.m_uid.m_nMediaType)
			&& (m_uid.m_ptrPersistent == rhs.m_uid.m_ptrPersistent)
			&& (m_uid.m_ptrVolatile == rhs.m_uid.m_ptrVolatile);
	}

	bool operator <(const NodePointer& rhs) const
	{
		return (m_uid.m_nMediaType < rhs.m_uid.m_nMediaType)
			&& (m_uid.m_ptrPersistent < rhs.m_uid.m_ptrPersistent)
			&& (m_uid.m_ptrVolatile < rhs.m_uid.m_ptrVolatile);
	}

	struct HashFunction
	{
	public:
		size_t operator()(const NodePointer& rhs) const
		{
			return std::hash<uint8_t>()(rhs.m_uid.m_nMediaType)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrPersistent)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrVolatile);
		}
	};

	struct EqualFunction
	{
	public:
		bool operator()(const NodePointer& lhs, const NodePointer& rhs) const {
			return (lhs.m_uid.m_nMediaType == rhs.m_uid.m_nMediaType)
				&& (lhs.m_uid.m_ptrPersistent == rhs.m_uid.m_ptrPersistent)
				&& (lhs.m_uid.m_ptrVolatile == rhs.m_uid.m_ptrVolatile);
		}
	};


public:
	NodePointer()
	{
	}
};

namespace std {
	template <>
	struct hash<NodePointer> {
		size_t operator()(const NodePointer& rhs) const
		{
			return std::hash<uint8_t>()(rhs.m_uid.m_nMediaType)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrPersistent)
				^ std::hash<uint32_t>()(rhs.m_uid.m_ptrVolatile);
		}
	};
}

