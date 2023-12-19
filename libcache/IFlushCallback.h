#pragma once
#include <unordered_map>
#include "CacheErrorCodes.h"

template <typename ObjectUIDType, typename ObjectType>
class IFlushCallback
{
protected:
	mutable std::shared_mutex m_mtxUIDsUpdate;
	std::unordered_map<ObjectUIDType, ObjectUIDType> m_mpUIDsUpdate;

public:
	virtual CacheErrorCode keyUpdate(std::shared_ptr<ObjectType> ptrParentNode, ObjectUIDType uidOld, ObjectUIDType uidNew) = 0;
	virtual CacheErrorCode keysUpdate(std::unordered_map<ObjectUIDType, ObjectUIDType> mpUpdatedUIDs) = 0;
};