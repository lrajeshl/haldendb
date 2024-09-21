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
class FileStorage
{
	typedef FileStorage<ICallback, KeyType, ValueType, CoreTypesMarshaller, ValueCoreTypes...> SelfType;

public:
	typedef KeyType ObjectUIDType;
	typedef ValueType<CoreTypesMarshaller, ValueCoreTypes...> ObjectType;

private:
	size_t m_nFileSize;
	size_t m_nBlockSize;

	std::string m_stFilename;
	std::fstream m_fsStorage;

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
	~FileStorage()
	{
#ifdef __CONCURRENT__
		m_bStopFlush = true;
		//m_threadBatchFlush.join();

		m_mpObjects.clear();
#endif __CONCURRENT__

		m_fsStorage.close();
	}

	FileStorage(size_t nBlockSize, size_t nFileSize, const std::string& stFilename)
		: m_nFileSize(nFileSize)
		, m_nBlockSize(nBlockSize)
		, m_stFilename(stFilename)
		, m_nNextBlock(0)
		, m_ptrCallback(NULL)
	{
		m_vtAllocationTable.resize(nFileSize/nBlockSize, false);

		//m_fsStorage.rdbuf()->pubsetbuf(0, 0);
		m_fsStorage.open(stFilename.c_str(), std::ios::out | std::ios::binary);
		m_fsStorage.close();

		m_fsStorage.open(stFilename.c_str(), std::ios::out | std::ios::binary | std::ios::in);
		m_fsStorage.seekp(0);
		m_fsStorage.seekg(0);

		if (!m_fsStorage.is_open())
		{
			throw new std::logic_error("should not occur!");   // TODO: critical log.
		}

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
		//char* szBuffer = new char[uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1]; //2
		//memset(szBuffer, '\0', uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1); //2

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		m_fsStorage.seekg(uidObject.m_uid.FATPOINTER.m_ptrFile.m_nOffset);
		std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(m_fsStorage); //1
		//m_fsStorage.read(szBuffer, uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize); //2

#ifdef __CONCURRENT__
		lock_file_storage.unlock();
#endif __CONCURRENT__


		//std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(szBuffer); //2
		
		ptrObject->setDirtyFlag( false);

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
		
		//char* szBuffer = NULL; //2
		//ptrObject->serialize(szBuffer, uidObjectType, nBufferSize); //2

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		m_fsStorage.seekp(m_nNextBlock * m_nBlockSize);
		ptrObject->serialize(m_fsStorage, uidObjectType, nBufferSize); //1
		//m_fsStorage.write(szBuffer, nBufferSize); //2
		m_fsStorage.flush();

		size_t nRequiredBlocks = std::ceil((nBufferSize + sizeof(uint8_t)) / (float)m_nBlockSize);

#ifdef __CONCURRENT__
		lock_file_storage.unlock();
#endif __CONCURRENT__

		//delete[] szBuffer; //2

		uidUpdated = ObjectUIDType::createAddressFromFileOffset(m_nNextBlock, m_nBlockSize, nBufferSize + sizeof(uint8_t));

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
		return ObjectUIDType::File;
	}

	CacheErrorCode addObjects(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects, size_t nNewOffset)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		m_nNextBlock = nNewOffset;

		auto it = vtObjects.begin();
		while (it != vtObjects.end())
		{
				m_fsStorage.seekp((*(*it).second.first).m_uid.FATPOINTER.m_ptrFile.m_nOffset);
				//m_fsStorage.write( vtBuffer[idx], (*(m_vtObjects[idx].uidDetails.uidObject_Updated)).m_uid.FATPOINTER.m_ptrFile.m_nSize); //2

				size_t nBufferSize = 0;
				uint8_t uidObjectType = 0;

				(*it).second.second->serialize(m_fsStorage, uidObjectType, nBufferSize);

				//vtUIDUpdates.push_back(std::move(m_vtObjects[idx].uidDetails));

				//delete[] vtBuffer[idx];

			it++;
		}
		m_fsStorage.flush();

		return CacheErrorCode::Success;
	}

#ifdef __CONCURRENT__
	void performBatchFlush()
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
	}

	static void handlerBatchFlush(SelfType* ptrSelf)
	{
		do
		{
			ptrSelf->performBatchFlush();

			std::this_thread::sleep_for(100ms);

		} while (!ptrSelf->m_bStopFlush);
	}
#endif __CONCURRENT__
};

