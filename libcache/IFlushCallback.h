#pragma once
#include <thread>
#include <unordered_map>
#include "CacheErrorCodes.h"
#include "ObjectFatUID.h"

template <typename ObjectUIDType, typename ObjectType>
class IFlushCallback
{
public:
	bool m_bStopProcessUIDUpdates;
	std::thread m_tdProcessUIDUpdates;

	mutable std::shared_mutex m_mtxUIDUpdates;
	std::vector<UIDUpdateRequest> m_vtUIDUpdates;

public:
	virtual CacheErrorCode updateChildUID(const std::optional<ObjectUIDType>& uidObject, const ObjectUIDType& uidChildOld, const ObjectUIDType& uidChildNew) = 0;
	virtual CacheErrorCode updateChildUID(std::vector<UIDUpdateRequest>& vtUIDUpdates) = 0;

	virtual void prepareFlush(
		std::vector<ObjectFlushRequest<ObjectType>>& vtObjects,
		size_t& nOffset,
		size_t nBlockSize,
		std::unordered_map<ObjectUIDType, std::shared_ptr<UIDUpdate>>& mpUIDUpdates) = 0;
};