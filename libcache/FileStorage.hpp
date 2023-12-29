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

#define __CONCURRENT__
#define __POSITION_AWARE_ITEMS__

template<
	typename ICallback,
	typename ObjectUIDType, 
	template <typename, typename...> typename ObjectType, 
	typename CoreTypesMarshaller, 
	typename... ObjectCoreTypes
>
class FileStorage
{
	typedef FileStorage<ICallback, ObjectUIDType, ObjectType, CoreTypesMarshaller, ObjectCoreTypes...> SelfType;

public:
	typedef ObjectUIDType ObjectUIDType;
	typedef ObjectType<CoreTypesMarshaller, ObjectCoreTypes...> ObjectType;

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

	mutable std::shared_mutex m_mtxCache;

	mutable std::shared_mutex m_mtxFile;

	std::vector<ObjectFlushRequest<ObjectType>> m_vtObjects;
#endif __CONCURRENT__

public:
	~FileStorage()
	{
#ifdef __CONCURRENT__
		m_bStopFlush = true;
		m_threadBatchFlush.join();

		m_vtObjects.clear();
#endif __CONCURRENT__
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
		m_fsStorage.open(stFilename.c_str(), std::ios::binary | std::ios::in | std::ios::out);
		
		if (!m_fsStorage.is_open())
		{
			throw new exception("should not occur!");   // TODO: critical log.
		}

#ifdef __CONCURRENT__
		m_bStopFlush = false;
		m_threadBatchFlush = std::thread(handlerBatchFlush, this);
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
//#ifdef __CONCURRENT__
//		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxCache);
//
//		if (m_mpObjects.find(uidObject) == m_mpObjects.end())
//		{
//			throw new std::exception("should not occur!");
//		}
//
//		std::shared_ptr<ObjectType> ptrObject = m_mpObjects[uidObject];
//		m_mpObjects.erase(uidObject);
//
//		return ptrObject;
//
//		lock_file_storage.unlock();
//#endif __CONCURRENT__

		std::unique_lock<std::shared_mutex> lock_file(m_mtxFile);

		m_fsStorage.seekg(uidObject.m_uid.FATPOINTER.m_ptrFile.m_nOffset);

		char* szBuffer = new char[uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1]; //2
		memset(szBuffer, '\0', uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1); //2
		m_fsStorage.read(szBuffer, uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize); //2

		lock_file.unlock();

		//std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(m_fsStorage); //1
		std::shared_ptr<ObjectType> ptrObject = std::make_shared<ObjectType>(szBuffer); //2
		ptrObject->dirty = false;

		delete[] szBuffer; //2

		return ptrObject;
	}

	CacheErrorCode remove(const ObjectUIDType& ptrKey)
	{
		throw new std::exception("no implementation!");
	}

#ifdef __POSITION_AWARE_ITEMS__ 
#ifdef __CONCURRENT__
	CacheErrorCode addObjects(std::vector<ObjectFlushRequest<ObjectType>>& vtObjects, size_t nNewOffset)
	{
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxCache);

		m_vtObjects.insert(m_vtObjects.end(), std::make_move_iterator(vtObjects.begin()), std::make_move_iterator(vtObjects.end()));

		m_nNextBlock = nNewOffset;

		//if (m_vtObjects.find(uidObject) != m_vtObjects.end())
		//{
		//	throw new std::exception("should not occur!");
		//}

		return CacheErrorCode::Success;
	}
#endif __CONCURRENT__
#endif __POSITION_AWARE_ITEMS__

#ifdef __POSITION_AWARE_ITEMS__
	CacheErrorCode addObject(const ObjectUIDType& uidObject, const std::shared_ptr<ObjectType> ptrObject, const std::optional<ObjectUIDType>& uidParent)
#else
	CacheErrorCode addObject(const ObjectUIDType& uidObject, const std::shared_ptr<ObjectType> ptrObject)
#endif __POSITION_AWARE_ITEMS__
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxCache);

		//if (m_vtObjects.find(uidObject) != m_vtObjects.end())
		//{
		//	throw new std::exception("should not occur!");
		//}

		m_vtObjects.push_back(std::make_pair(uidObject, std::make_pair( uidParent, ptrObject)));

		return CacheErrorCode::Success;
#else __CONCURRENT__

		size_t nBufferSize = 0;
		uint8_t uidObjectType = 0;
		
		//char* szBuffer = NULL; //2
		//ptrObject->serialize(szBuffer, uidObjectType, nBufferSize); //2

		m_fsStorage.seekp(m_nNextBlock * m_nBlockSize);
		ptrObject->serialize(m_fsStorage, uidObjectType, nBufferSize); //1
		//m_fsStorage.write(szBuffer, nBufferSize); //2

		//std::streampos writePos = m_fsStorage.tellp();
		//std::size_t dataSize = static_cast<std::size_t>(writePos) - m_nNextBlock * m_nBlockSize;
		//assert(nBufferSize == dataSize);

		m_fsStorage.flush();

		//delete[] szBuffer; //2

		size_t nRequiredBlocks = std::ceil( (nBufferSize + sizeof(uint8_t)) / (float)m_nBlockSize);

		ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromFileOffset(m_nNextBlock * m_nBlockSize, nBufferSize + sizeof(uint8_t));

		for (int idx = 0; idx < nRequiredBlocks; idx++)
		{
			m_vtAllocationTable[m_nNextBlock++] = true;
		}

#ifdef __POSITION_AWARE_ITEMS__
		m_ptrCallback->updateChildUID(uidParent, uidObject, uidUpdated);
#endif __POSITION_AWARE_ITEMS__

#endif __CONCURRENT__

		return CacheErrorCode::Success;
	}

#ifdef __CONCURRENT__
	void performBatchFlush()
	{
		std::vector<char*> vtBuffer;

		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxCache);
		if (m_vtObjects.size() == 0)
			return;


		auto it = m_vtObjects.begin();
		while (it != m_vtObjects.end())
		{
			size_t nBufferSize = 0;
			uint8_t uidObjectType = 0;

			char* szBuffer = NULL; //2
			(*it).ptrObject->serialize(szBuffer, uidObjectType, nBufferSize); //2

			//m_fsStorage.seekp(m_nNextBlock * m_nBlockSize);
			//m_fsStorage.write(szBuffer, nBufferSize); //2

			//lock_file.unlock();

			//vtBuffer.push_back(std::make_pair(m_nNextBlock * m_nBlockSize, szBuffer));
			//delete[] szBuffer; //2			//? use the same buffer? 1MB size?

			//size_t nRequiredBlocks = std::ceil(nBufferSize / (float)m_nBlockSize);

			//ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromFileOffset(m_nNextBlock * m_nBlockSize, nBufferSize + sizeof(uint8_t));

			vtBuffer.push_back(szBuffer);
			//(*it).uidDetails.uidObject_Updated = uidUpdated;

			//for (int idx = 0; idx < nRequiredBlocks; idx++)
			{
			//	m_vtAllocationTable[m_nNextBlock++] = true;
			}
			it++;
		}

		std::unique_lock<std::shared_mutex> lock_file(m_mtxFile);

		std::vector<UIDUpdateRequest> vtUIDUpdates;

		for (int idx = 0; idx < m_vtObjects.size(); idx++)
		{
			m_fsStorage.seekp((*(m_vtObjects[idx].uidDetails.uidObject_Updated)).m_uid.FATPOINTER.m_ptrFile.m_nOffset);
			m_fsStorage.write( vtBuffer[idx], (*(m_vtObjects[idx].uidDetails.uidObject_Updated)).m_uid.FATPOINTER.m_ptrFile.m_nSize); //2
		
			vtUIDUpdates.push_back(std::move(m_vtObjects[idx].uidDetails));

			delete[] vtBuffer[idx];
		}

		m_fsStorage.flush();
		m_vtObjects.clear();

		lock_file.unlock();
		lock_file_storage.unlock();

		m_ptrCallback->updateChildUID(vtUIDUpdates);
	}

	inline size_t getWritePos()
	{
		return m_nNextBlock;
	}

	inline size_t getBlockSize()
	{
		return m_nBlockSize;
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

