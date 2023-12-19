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
#include "VariadicNthType.h"

class TypeProcessor {
public:
	void process(int value) const {
		std::cout << "Processing int: " << value << std::endl;
	}

	void process(double value) const {
		std::cout << "Processing double: " << value << std::endl;
	}

	void process(const std::string& value) const {
		std::cout << "Processing string: " << value << std::endl;
	}

};

template<
	typename ICallback,
	typename ObjectUIDType, 
	template <typename, typename...> typename ObjectWrapperType, 
	typename ObjectTypeMarshaller, 
	typename... ObjectTypes
>
class FileStorage
{
	using type = typename NthType<0, ObjectTypes...>::type;
	using type2 = typename NthType<1, ObjectTypes...>::type;
	//using type21 = typename NthType<2, ObjectTypes...>::type;

	typedef FileStorage<ICallback, ObjectUIDType, ObjectWrapperType, ObjectTypeMarshaller, ObjectTypes...> SelfType;

public:
	typedef ObjectUIDType ObjectUIDType;
	typedef ObjectWrapperType<ObjectTypeMarshaller, ObjectTypes...> ObjectType;
	typedef std::tuple<ObjectTypes...> ObjectCoreTypes;

private:
	size_t m_nFileSize;
	size_t m_nBlockSize;

	std::string m_stFilename;
	std::fstream m_fsStorage;

	size_t m_nNextBlock;
	std::vector<bool> m_vtAllocationTable;

	mutable std::shared_mutex m_mtxStorage;

	bool m_bStop;
	std::thread m_threadBatchFlush;


	std::unordered_map<ObjectUIDType, std::shared_ptr<ObjectType>> m_mpObject;

	ICallback* m_ptrCallback;

public:
	~FileStorage()
	{
		m_bStop = true;
		m_threadBatchFlush.join();

		m_mpObject.clear();
	}

	FileStorage(size_t nBlockSize, size_t nFileSize, std::string stFilename)
		: m_nFileSize(nFileSize)
		, m_nBlockSize(nBlockSize)
		, m_stFilename(stFilename)
		, m_nNextBlock(0)
		, m_bStop(false)
	{
		m_vtAllocationTable.resize(nFileSize/nBlockSize, false);

		//m_fsStorage.rdbuf()->pubsetbuf(0, 0);
		m_fsStorage.open(stFilename.c_str(), std::ios::binary | std::ios::in | std::ios::out);
		
		if (!m_fsStorage.is_open())
		{
			throw new exception("should not occur!");   // TODO: critical log.
		}

		//m_threadBatchFlush = std::thread(handlerBatchFlush, this);
	}

	template <typename... InitArgs>
	CacheErrorCode init(ICallback* ptrCallback, InitArgs... args)
	{
		m_ptrCallback = ptrCallback;// getNthElement<0>(args...);
		return CacheErrorCode::Success;
	}

	std::shared_ptr<ObjectType> getObject(ObjectUIDType uidObject)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);

		if (m_mpObject.find(uidObject) == m_mpObject.end())
		{
			throw new std::exception("should not occur!");
		}

		std::shared_ptr<ObjectType> ptrObject = m_mpObject[uidObject];
		m_mpObject.erase(uidObject);

		return ptrObject;

		lock_file_storage.unlock();
#endif __CONCURRENT__

		// Fetch from file.

		char* szBuffer = new char[m_nBlockSize];
		memset(szBuffer, 0, m_nBlockSize);

		m_fsStorage.seekg(uidObject.m_uid.FATPOINTER.m_ptrFile.m_nOffset);
		m_fsStorage.read(szBuffer, uidObject.m_uid.FATPOINTER.m_ptrFile.m_nSize);

		std::shared_ptr<ObjectType> ptrDeserializedObject = std::make_shared<ObjectType>(szBuffer);

		return ptrDeserializedObject;
	}

	CacheErrorCode addObject(ObjectUIDType uidObject, std::shared_ptr<ObjectType> ptrObject)
	{
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);

		//if (m_mpObject.find(ptrKey) != m_mpObject.end())
		//{
		//	throw new std::exception("should not occur!");
		//}

		m_mpObject[uidObject] = ptrObject;

		return CacheErrorCode::Success;
#else __CONCURRENT__
		size_t nDataSize = 0;
		uint8_t uidObjectType = 0;

		const char* szData = ptrObject->serialize(uidObjectType, nDataSize);

		m_fsStorage.seekp(m_nNextBlock * m_nBlockSize);
		m_fsStorage.write((const char*)&uidObjectType, sizeof(uint8_t));
		m_fsStorage.write((const char*)(szData), nDataSize);

		size_t nBlockRequired = std::ceil( (nDataSize + sizeof(uint8_t)) / (float)m_nBlockSize);

		ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromFileOffset(m_nNextBlock * m_nBlockSize, nDataSize + sizeof(uint8_t));

		for (int idx = 0; idx < nBlockRequired; idx++)
		{
			m_vtAllocationTable[m_nNextBlock++] = true;
		}
	
		m_fsStorage.flush();

		m_ptrCallback->keyUpdate(ptrObject, uidObject, uidUpdated);

		return CacheErrorCode::Success;
#endif __CONCURRENT__
	}

	void performBatchFlush() 
	{
		std::unordered_map<ObjectUIDType, ObjectUIDType> mpUpdatedUIDs;
#ifdef __CONCURRENT__
		std::unique_lock<std::shared_mutex> lock_file_storage(m_mtxStorage);
#endif __CONCURRENT__

		auto it = m_mpObject.begin();
		while (it != m_mpObject.end())
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

		} while (!ptrSelf->m_bStop);
	}
};

