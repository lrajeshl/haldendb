#pragma once
#include <memory>
#include <iostream>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <variant>
#include <cmath>

#include "ErrorCodes.h"
#include "IFlushCallback.h"

template<
	typename ICallback,
	typename KeyType,
	template <typename, typename...> typename ValueType,
	typename CoreTypesMarshaller,
	typename... ValueCoreTypes
>
class VolatileStorage
{
	typedef VolatileStorage<ICallback, KeyType, ValueType, CoreTypesMarshaller, ValueCoreTypes...> SelfType;

public:
	typedef KeyType ObjectUIDType;
	typedef ValueType<CoreTypesMarshaller, ValueCoreTypes...> ObjectType;

private:
	char* m_szStorage;
	size_t m_nStorageSize;
	size_t m_nBlockSize;

	size_t m_nNextBlock;
	std::vector<bool> m_vtAllocationTable;

	ICallback* m_ptrCallback;

#ifdef __CONCURRENT__
	mutable std::shared_mutex m_mtxStorage;
#endif __CONCURRENT__

public:
	~VolatileStorage()
	{
		delete[] m_szStorage;
	}

	VolatileStorage(size_t nBlockSize, size_t nStorageSize)
		: m_nStorageSize(nStorageSize)
		, m_nBlockSize(nBlockSize)
		, m_nNextBlock(0)
		, m_ptrCallback(NULL)
	{
		m_szStorage = new(std::nothrow) char[m_nStorageSize];
		memset(m_szStorage, 0, m_nStorageSize);

		if (m_szStorage == nullptr)
		{
			throw new std::logic_error("should not occur!"); // TODO: critical log.
		}

		m_vtAllocationTable.resize(nStorageSize / nBlockSize, false);
	}

	template <typename... InitArgs>
	CacheErrorCode init(ICallback* ptrCallback, InitArgs... args)
	{
		m_ptrCallback = ptrCallback;// getNthElement<0>(args...);
		return CacheErrorCode::Success;
	}

	std::shared_ptr<ObjectType> getObject(const ObjectUIDType& uidObject)
	{
		//char* szBuffer = new char[uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1];
		//memset(szBuffer, 0, uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1);
		std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(m_szStorage + uidObject.m_uid.FATPOINTER.m_ptrFile.m_nOffset);

		//ptrObject->setDirtyFlag(false);
		//delete[] szBuffer;

		return ptrObject;
	}

	CacheErrorCode remove(const ObjectUIDType& ptrKey)
	{
		//throw new std::logic_error("no implementation!");
		return CacheErrorCode::Success;
	}

	CacheErrorCode addObject(ObjectUIDType uidObject, std::shared_ptr<ObjectType> ptrObject, ObjectUIDType& uidUpdated)
	{
		size_t nBufferSize = 0;
		uint8_t uidObjectType = 0;

		char* szBuffer = NULL;
		ptrObject->serialize(szBuffer, uidObjectType, nBufferSize);

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_dram_storage(m_mtxStorage);
#endif __CONCURRENT__

		memcpy(m_szStorage + (m_nNextBlock * m_nBlockSize), szBuffer, nBufferSize);

		size_t nNextBlockOld = m_nNextBlock;
		//size_t nRequiredBlocks = std::ceil((nBufferSize + sizeof(uint8_t)) / (float)m_nBlockSize);
		//for (int idx = 0; idx < nRequiredBlocks; idx++)
		//{
		//	m_vtAllocationTable[m_nNextBlock++] = true;
		//}
		m_nNextBlock += std::ceil((nBufferSize + sizeof(uint8_t)) / (float)m_nBlockSize);;

#ifdef __CONCURRENT__
		lock_dram_storage.unlock();
#endif __CONCURRENT__

		delete[] szBuffer;

		uidUpdated = ObjectUIDType::createAddressFromFileOffset(uidObject.m_uid.m_nType, nNextBlockOld, m_nBlockSize, nBufferSize + sizeof(uint8_t));

		return CacheErrorCode::Success;
	}

	inline size_t getWritePos() const
	{
		return m_nNextBlock;
	}

	inline size_t getBlockSize() const
	{
		return m_nBlockSize;
	}

	inline ObjectUIDType::Media getMediaType()
	{
		return ObjectUIDType::DRAM;
	}

	CacheErrorCode addObjects(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects, size_t nNewOffset)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_dram_storage(m_mtxStorage);
#endif __CONCURRENT__

		for (auto it = vtObjects.begin(); it != vtObjects.end(); it++)
		{
			uint32_t nBufferSize = 0;
			uint8_t uidObjectType = 0;

			char* szBuffer = NULL;
			(*it).second.second->serialize(szBuffer, uidObjectType, nBufferSize);
			memcpy(m_szStorage + (*(*it).second.first).m_uid.FATPOINTER.m_ptrFile.m_nOffset, szBuffer, nBufferSize);

			delete[] szBuffer;
		}

		m_nNextBlock = nNewOffset;

		return CacheErrorCode::Success;
	}
};
