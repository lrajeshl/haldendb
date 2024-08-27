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
	typename ObjectUIDType_,
	template <typename, typename...> typename ObjectType_,
	typename CoreTypesMarshaller,
	typename... ObjectCoreTypes
>
class VolatileStorage
{
	typedef VolatileStorage<ICallback, ObjectUIDType_, ObjectType_, CoreTypesMarshaller, ObjectCoreTypes...> SelfType;

public:
	typedef ObjectUIDType_ ObjectUIDType;
	typedef ObjectType_<CoreTypesMarshaller, ObjectCoreTypes...> ObjectType;

private:
	char* m_szStorage;
	size_t m_nStorageSize;
	size_t m_nBlockSize;

	size_t m_nNextBlock;
	std::vector<bool> m_vtAllocationTable;

	ICallback* m_ptrCallback;

#ifdef __CONCURRENT__
	bool m_bStopFlush;
	std::thread m_threadBatchFlush;

	mutable std::shared_mutex m_mtxFile;
	mutable std::shared_mutex m_mtxStorage;

	std::unordered_map<ObjectUIDType, std::shared_ptr<ObjectType>> m_mpObjects;
#endif __CONCURRENT__

public:
	~VolatileStorage()
	{
		delete[] m_szStorage;

#ifdef __CONCURRENT__
		m_bStopFlush = true;
		//m_threadBatchFlush.join();

		m_mpObjects.clear();
#endif __CONCURRENT__
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


#ifdef __CONCURRENT__
		m_bStopFlush = false;
		//m_threadBatchFlush = std::thread(handlerBatchFlush, this);
#endif __CONCURRENT__
	}

	template <typename... InitArgs>
	CacheErrorCode init(ICallback* ptrCallback, InitArgs... args)
	{
		m_ptrCallback = ptrCallback;// getNthElement<0>(args...);
		return CacheErrorCode::Success;
	}

	std::shared_ptr<ObjectType> getObject(const ObjectUIDType& uidObject)
	{
		char* szBuffer = new char[uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1]; //2
		memset(szBuffer, 0, uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1); //2
		std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(m_szStorage + uidObject.m_uid.FATPOINTER.m_ptrFile.m_nOffset); //1
/* COW!
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		//m_fsStorage.seekg(uidObject.m_uid.FATPOINTER.m_ptrFile.m_nOffset);
		//std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(m_fsStorage); //1
		//m_fsStorage.read(szBuffer, uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize); //2

#ifdef __CONCURRENT__
		lock_file_storage.unlock();
#endif __CONCURRENT__
*/

//std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(szBuffer); //2

		ptrObject->dirty = false;

		//delete[] szBuffer; //2

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

		char* szBuffer = NULL; //2
		ptrObject->serialize(szBuffer, uidObjectType, nBufferSize); //2

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		memcpy(m_szStorage + (m_nNextBlock * m_nBlockSize), szBuffer, nBufferSize);

		//m_fsStorage.seekp(m_nNextBlock * m_nBlockSize);
		//ptrObject->serialize(m_fsStorage, uidObjectType, nBufferSize); //1
		//m_fsStorage.write(szBuffer, nBufferSize); //2
		//m_fsStorage.flush();

		size_t nRequiredBlocks = std::ceil((nBufferSize + sizeof(uint8_t)) / (float)m_nBlockSize);

#ifdef __CONCURRENT__
		lock_file_storage.unlock();
#endif __CONCURRENT__

		delete[] szBuffer; //2

		uidUpdated = ObjectUIDType::createAddressFromFileOffset(uidObject.m_nType, m_nNextBlock, m_nBlockSize, nBufferSize + sizeof(uint8_t));

		for (int idx = 0; idx < nRequiredBlocks; idx++)
		{
			m_vtAllocationTable[m_nNextBlock++] = true;
		}

		return CacheErrorCode::Success;
	}

	inline size_t getWritePos()
	{
		return m_nNextBlock;
	}

	inline size_t getBlockSize()
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
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		m_nNextBlock = nNewOffset;
		int j = 0;
		auto it = vtObjects.begin();
		while (it != vtObjects.end())
		{
			size_t nBufferSize = 0;
			uint8_t uidObjectType = 0;

			char* szBuffer = NULL; //2
			(*it).second.second->serialize(szBuffer, uidObjectType, nBufferSize); //2
			memcpy(m_szStorage + (*(*it).second.first).m_uid.FATPOINTER.m_ptrFile.m_nOffset, szBuffer, nBufferSize);

			//m_fsStorage.seekp((*(*it).second.first).m_uid.FATPOINTER.m_ptrFile.m_nOffset);
			//m_fsStorage.write( vtBuffer[idx], (*(m_vtObjects[idx].uidDetails.uidObject_Updated)).m_uid.FATPOINTER.m_ptrFile.m_nSize); //2

			//size_t nBufferSize = 0;
			//uint8_t uidObjectType = 0;

			//(*it).second.second->serialize(m_fsStorage, uidObjectType, nBufferSize);

			//vtUIDUpdates.push_back(std::move(m_vtObjects[idx].uidDetails));

			//delete[] vtBuffer[idx];

			delete[] szBuffer;
			j++;
			it++;
		}
		//m_fsStorage.flush();

		return CacheErrorCode::Success;
	}

#ifdef __CONCURRENT__
	/*void performBatchFlush()
	{
		std::unordered_map<ObjectUIDType, ObjectUIDType> mpUpdatedUIDs;
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		auto it = m_mpObjects.begin();
		while (it != m_mpObjects.end())
		{
			std::tuple<uint8_t, const std::byte*, size_t> tpSerializedData = it->second->serialize();

			m_fsStorage.seekp(m_nNextBlock * m_nBlockSize);
			m_fsStorage.write((char*)(&std::get<0>(tpSerializedData)), sizeof(uint8_t));
			m_fsStorage.write((char*)(std::get<1>(tpSerializedData)), std::get<2>(tpSerializedData));

			size_t nBlockRequired = std::ceil(std::get<2>(tpSerializedData) / (float)m_nBlockSize);

			ObjectUIDType uid = ObjectUIDType::createAddressFromFileOffset(m_nBlockSize, nBlockRequired * m_nBlockSize);
			mpUpdatedUIDs[it->first] = uid;

			for (int idx = 0; idx < nBlockRequired; idx++)
			{
				m_vtAllocationTable[m_nNextBlock++] = true;
			}
		}
		m_fsStorage.flush();

		m_ptrCallback->keysUpdate(mpUpdatedUIDs);
	}*/

	static void handlerBatchFlush(SelfType* ptrSelf)
	{
		do
		{
			//ptrSelf->performBatchFlush();

			std::this_thread::sleep_for(100ms);

		} while (!ptrSelf->m_bStopFlush);
	}
#endif __CONCURRENT__
};
