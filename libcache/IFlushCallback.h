#pragma once
#include <unordered_map>
#include "CacheErrorCodes.h"
#include <optional>

template <typename ObjectUIDType, typename ObjectType>
class IFlushCallback
{
public:
#ifdef __TRACK_CACHE_FOOTPRINT__
	virtual void applyExistingUpdates(std::shared_ptr<ObjectType> ptrObject
		, std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates, int32_t& nMemoryFootprint) = 0;

	virtual void applyExistingUpdates(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects
		, std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates, int32_t& nMemoryFootprint) = 0;

	virtual void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects
		, size_t nOffset, size_t& nNewOffset, uint16_t nBlockSize, ObjectUIDType::StorageMedia nMediaType, int32_t& nMemoryFootprint) = 0;
#else //__TRACK_CACHE_FOOTPRINT__
	virtual void applyExistingUpdates(std::shared_ptr<ObjectType> ptrObject
		, std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates) = 0;

	virtual void applyExistingUpdates(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects
		, std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates) = 0;

	virtual void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects
		, size_t nOffset, size_t& nNewOffset, uint16_t nBlockSize, ObjectUIDType::StorageMedia nMediaType) = 0;
#endif //__TRACK_CACHE_FOOTPRINT__

};
