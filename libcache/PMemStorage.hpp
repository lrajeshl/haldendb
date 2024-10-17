#pragma once
#include <memory>
#include <iostream>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <variant>
#include <cmath>
#include <libpmem.h>

bool createMMapFile(void*& hMemory, const char* szPath, size_t nFileSize, size_t& nMappedLen, int& bIsPMem)
{
	if ((hMemory = pmem_map_file(szPath,
		nFileSize,
		PMEM_FILE_CREATE | PMEM_FILE_EXCL,
		0666, &nMappedLen, &bIsPMem)) == NULL)
	{
		return false;
	}

	return true;
}

bool openMMapFile(void*& hMemory, const char* szPath, size_t& nMappedLen, int& bIsPMem)
{
	if ((hMemory = pmem_map_file(szPath,
		0,
		0,
		0666, &nMappedLen, &bIsPMem)) == NULL)
	{
		return false;
	}

	return true;
}

bool writeMMapFile(void* hMemory, const char* szBuf, size_t nLen)
{
	void* hDestBuf = pmem_memcpy_persist(hMemory, szBuf, nLen);

	if (hDestBuf == NULL)
	{
		return false;
	}

	return true;
}

bool readMMapFile(const void* hMemory, char* szBuf, size_t nLen)
{
	void* hDestBuf = pmem_memcpy(szBuf, hMemory, nLen, PMEM_F_MEM_NOFLUSH);

	if (hDestBuf == NULL)
	{
		return false;
	}

	return true;
}

void closeMMapFile(void* hMemory, size_t nMappedLen)
{
	pmem_unmap(hMemory, nMappedLen);
}

template<
	typename ICallback,
	typename ObjectUIDType_,
	template <typename, typename...> typename ObjectType_,
	typename CoreTypesMarshaller,
	typename... ObjectCoreTypes
>
class PMemStorage
{
	typedef PMemStorage<ICallback, ObjectUIDType_, ObjectType_, CoreTypesMarshaller, ObjectCoreTypes...> SelfType;

public:
	typedef ObjectUIDType_ ObjectUIDType;
	typedef ObjectType_<CoreTypesMarshaller, ObjectCoreTypes...> ObjectType;

private:
	int nIsPMem;
	size_t nMappedLen;
	void* hMemory = NULL;

	size_t m_nNextBlock;

	size_t m_nBlockSize;
	size_t m_nStorageSize;
	std::string m_stFilename;

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
	~PMemStorage()
	{
		closeMMapFile(hMemory, nMappedLen);

#ifdef __CONCURRENT__
		m_bStopFlush = true;
		//m_threadBatchFlush.join();

		m_mpObjects.clear();
#endif __CONCURRENT__
	}

	PMemStorage(size_t nBlockSize, size_t nStorageSize, const std::string& stFilename)
		: m_nStorageSize(nStorageSize)
		, m_nBlockSize(nBlockSize)
		, m_stFilename(stFilename)
		, m_nNextBlock(0)
		, nMappedLen(0)		
		, hMemory (nullptr)
		, m_ptrCallback(NULL)
	{
		nStorageSize = 10ULL *1024*1024*1024;
		m_nStorageSize = 10ULL*1024*1024*1024;

		if( !openMMapFile(hMemory, stFilename.c_str(), nMappedLen, nIsPMem))
		{
			std::cout << stFilename.c_str() << std::endl;
			std::cout << nStorageSize << std::endl;
			if( !createMMapFile(hMemory, stFilename.c_str(), nStorageSize, nMappedLen, nIsPMem))
			{
				std::cout << "Critical State: " << ".27." << std::endl;
				throw new std::logic_error(".....");   // TODO: critical log.
			}
		}

		if (hMemory == nullptr)
		{
			std::cout << "Critical State: " << ".28." << std::endl;
			throw new std::logic_error(".....");   // TODO: critical log.
		}

		if (nMappedLen != nStorageSize)
		{
			std::cout << "Critical State: " << ".29." << std::endl;
			throw new std::logic_error(".....");   // TODO: critical log.
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
		//return std::make_shared<ObjectType>(m_szStorage + uidObject.getPersistentPointerValue());

		//char* szBuffer = new char[uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1]; //2
		//memset(szBuffer, 0, uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize + 1); //2
		return /*std::shared_ptr<ObjectType> ptrObject =*/ 
			std::make_shared<ObjectType>((char*)hMemory + uidObject.getPersistentPointerValue()); //1



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

		//ptrObject->setDirtyFlag( false);

		//delete[] szBuffer; //2

		//return ptrObject;
	}

	CacheErrorCode remove(const ObjectUIDType& ptrKey)
	{
		//throw new std::logic_error("no implementation!");
		return CacheErrorCode::Success;
	}

	CacheErrorCode addObject(const ObjectUIDType& uidObject, std::shared_ptr<ObjectType> ptrObject, ObjectUIDType& uidUpdated)
	{
		uint32_t nBufferSize = 0;
		uint8_t uidObjectType = 0;

		char* szBuffer = NULL;
		ptrObject->serialize(szBuffer, uidObjectType, nBufferSize);

		size_t nOffset = m_nNextBlock * m_nBlockSize;

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_storage(m_mtxStorage);
#endif __CONCURRENT__

		//memcpy(m_szStorage + nOffset, szBuffer, nBufferSize);
		if (!writeMMapFile(hMemory + (m_nNextBlock * m_nBlockSize), szBuffer, nBufferSize))
		{
			std::cout << "Critical State: " << ".30." << std::endl;
			throw new std::logic_error(".....");   // TODO: critical log.
		}
		//m_nNextBlock += std::ceil((nBufferSize + sizeof(uint8_t)) / (float)m_nBlockSize);;
		m_nNextBlock += std::ceil(nBufferSize / (float)m_nBlockSize);

#ifdef __CONCURRENT__
		lock_storage.unlock();
#endif __CONCURRENT__

		delete[] szBuffer;

		ObjectUIDType::createAddressFromFileOffset(uidUpdated, uidObject.getObjectType(), nOffset, nBufferSize);

		return CacheErrorCode::Success;
	}

	/*CacheErrorCode addObject(ObjectUIDType uidObject, std::shared_ptr<ObjectType> ptrObject, ObjectUIDType& uidUpdated)
	{
		size_t nBufferSize = 0;
		uint8_t uidObjectType = 0;

		char* szBuffer = NULL; //2
		ptrObject->serialize(szBuffer, uidObjectType, nBufferSize); //2

#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		// memcpy(m_szStorage + (m_nNextBlock * m_nBlockSize), szBuffer, nBufferSize);
		if(!writeMMapFile(hMemory + ( m_nNextBlock * m_nBlockSize  ), szBuffer, nBufferSize))
		{
			throw new std::logic_error("failed to write data!");
		}
		
		

		//m_fsStorage.seekp(m_nNextBlock * m_nBlockSize);
		//ptrObject->serialize(m_fsStorage, uidObjectType, nBufferSize); //1
		//m_fsStorage.write(szBuffer, nBufferSize); //2
		//m_fsStorage.flush();

		size_t nRequiredBlocks = std::ceil((nBufferSize + sizeof(uint8_t)) / (float)m_nBlockSize);

#ifdef __CONCURRENT__
		lock_file_storage.unlock();
#endif __CONCURRENT__

		delete[] szBuffer; //2

		uidUpdated = ObjectUIDType::createAddressFromFileOffset(m_nNextBlock, m_nBlockSize, nBufferSize + sizeof(uint8_t));

		for (int idx = 0; idx < nRequiredBlocks; idx++)
		{
			m_vtAllocationTable[m_nNextBlock++] = true;
		}

		return CacheErrorCode::Success;
	}*/

	inline size_t getNextAvailableBlockOffset() const
	{
		return m_nNextBlock;
	}

	inline size_t getBlockSize() const
	{
		return m_nBlockSize;
	}

	inline ObjectUIDType::StorageMedia getStorageType()
	{
		return ObjectUIDType::PMem;
	}

	CacheErrorCode addObjects(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects, size_t nNewOffset)
	//CacheErrorCode addObjects(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects, size_t nNewOffset)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		m_nNextBlock = nNewOffset;

		auto it = vtObjects.begin();
		while (it != vtObjects.end())
		{
			uint32_t nBufferSize = 0;
			uint8_t uidObjectType = 0;

			char* szBuffer = NULL; //2
			(*it).second.second->serialize(szBuffer, uidObjectType, nBufferSize); //2
			
			//memcpy(m_szStorage + (*(*it).second.first).m_uid.FATPOINTER.m_ptrFile.m_nOffset, szBuffer, nBufferSize);
			if (!writeMMapFile(hMemory + (*(*it).second.first).getPersistentPointerValue(), szBuffer, nBufferSize))
			{
				std::cout << "Critical State: " << ".31." << std::endl;
				throw new std::logic_error(".....");   // TODO: critical log.
			}

			//m_fsStorage.seekp((*(*it).second.first).m_uid.FATPOINTER.m_ptrFile.m_nOffset);
			//m_fsStorage.write( vtBuffer[idx], (*(m_vtObjects[idx].uidDetails.uidObject_Updated)).m_uid.FATPOINTER.m_ptrFile.m_nSize); //2

			//size_t nBufferSize = 0;
			//uint8_t uidObjectType = 0;

			//(*it).second.second->serialize(m_fsStorage, uidObjectType, nBufferSize);

			//vtUIDUpdates.push_back(std::move(m_vtObjects[idx].uidDetails));

			//delete[] vtBuffer[idx];

			delete[] szBuffer;

			it++;
		}
		//m_fsStorage.flush();

		m_nNextBlock = nNewOffset;

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

