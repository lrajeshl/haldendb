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


template <size_t N, typename... Types>
struct NthType;

template <typename FirstType, typename... RemainingTypes>
struct NthType<0, FirstType, RemainingTypes...> {
	using type = FirstType;
};

template <size_t N, typename FirstType, typename... RemainingTypes>
struct NthType<N, FirstType, RemainingTypes...> {
	using type = typename NthType<N - 1, RemainingTypes...>::type;
};

template <
	typename ICallback,
	typename KeyType, 
	template <typename, typename...> typename ValueType, 
	typename ValueCoreTypesMarshaller, 
	typename... ValueCoreTypes
>
class FileStorage
{
public:
	//using type_0 = typename NthType<0, ValueCoreTypes...>::type;
	//using type_1 = typename NthType<1, ValueCoreTypes...>::type;

	typedef KeyType CacheKeyType;
	typedef ValueType<ValueCoreTypesMarshaller, ValueCoreTypes...> CacheValueType;

private:
	size_t m_nFileSize;
	size_t m_nBlockSize;

	std::string m_stFilename;
	std::fstream m_fsStorage;

	size_t m_nNextBlock;
	std::vector<bool> m_vtAllocationTable;

	ICallback* m_ptrCallback;

public:
	FileStorage(size_t nBlockSize, size_t nFileSize, std::string stFilename)
		: m_nFileSize(nFileSize)
		, m_nBlockSize(nBlockSize)
		, m_stFilename(stFilename)
		, m_nNextBlock(0)
	{
		m_vtAllocationTable.resize(nFileSize/nBlockSize, false);

		//m_fsStorage.rdbuf()->pubsetbuf(0, 0);
		m_fsStorage.open(stFilename.c_str(), std::ios::binary | std::ios::in | std::ios::out);
		
		if (!m_fsStorage.is_open())
		{
			//throw new exception("should not occur!");   // TODO: critical log.
		}
	}

	CacheErrorCode init(ICallback* ptrCallback)
	{
		m_ptrCallback = ptrCallback;
		return CacheErrorCode::Success;
	}

	std::shared_ptr<CacheValueType> getObject(KeyType ptrKey)
	{
		char* szBuffer = new char[m_nBlockSize];

		//m_fsStorage.seekg(ptrKey->toFileAddress());
		//m_fsStorage.read(szBuffer, m_nBlockSize);

		return nullptr;
	}

	CacheErrorCode addObject(KeyType ptrKey, std::shared_ptr<CacheValueType> ptrValue)
	{
		std::tuple<uint8_t, const std::byte*, size_t> tpSerializedData = ptrValue->serialize();
		//std::string serializedValue = std::visit(ValueCoreTypesMarshaller{}, ptrValue);

		//std::byte* bStream = new std::byte[std::get<2>(tpSerializedData)];
		//size_t nStreamSize = new std::byte[std::get<2>(tpSerializedData)];

		m_fsStorage.seekp(m_nNextBlock * m_nBlockSize);
		m_fsStorage.write((char*)(& std::get<0>(tpSerializedData)), sizeof(uint8_t));
		m_fsStorage.write((char*)(std::get<1>(tpSerializedData)), std::get<2>(tpSerializedData));
		m_fsStorage.flush();

		size_t nBlockRequired = std::ceil(std::get<2>(tpSerializedData) / (float)m_nBlockSize);

		ptrKey = KeyType::createAddressFromFileOffset(m_nBlockSize, nBlockRequired * m_nBlockSize);

		for (int idx = 0; idx < nBlockRequired; idx++)
		{
			m_vtAllocationTable[m_nNextBlock++] = true;
		}

		m_ptrCallback->keyUpdate(ptrKey, ptrKey, ptrKey);

		/*memcpy(__, std::get<1>(tpSerializedData), std::get<2>(tpSerializedData));

		ptrValue->deserialize(std::get<0>(tpSerializedData), __);

		std::variant<int, double, std::string> myVariant;

		TypeProcessor typeProcessor;

		std::visit([&typeProcessor](const auto& value) {
			typeProcessor.process(value);
			}, myVariant);	
			*/
		//ptrValue->data->get();
		return CacheErrorCode::Success;
	}
};

