#pragma once
#include <variant>
#include <typeinfo>
#include <iostream>
#include <fstream>

class TypeMarshaller
{
public:
	template <typename... ObjectCoreTypes>
	static void serialize(std::fstream& os, const std::variant<std::shared_ptr<ObjectCoreTypes>...>& objVariant, uint8_t& uidObjectType, uint32_t& nnBufferLength)
	{
		std::visit([&os, &uidObjectType, &nnBufferLength](const auto& value) {
			value->writeToStream(os, uidObjectType, nnBufferLength);
			}, objVariant);
	}

	template <typename... ObjectCoreTypes>
	static void serialize(char*& szBuffer, const std::variant<std::shared_ptr<ObjectCoreTypes>...>& objVariant, uint8_t& uidObjectType, uint32_t& nnBufferLength)
	{
		std::visit([&szBuffer, &uidObjectType, &nnBufferLength](const auto& value) {
			value->serialize(szBuffer, uidObjectType, nnBufferLength);
			}, objVariant);
	}

	template <typename ObjectType, typename... ObjectCoreTypes>
	static void deserialize(std::fstream& is, ObjectType& ptrObject)
	{
		using TypeA = typename NthType<0, ObjectCoreTypes...>::type;
		using TypeB = typename NthType<1, ObjectCoreTypes...>::type;

		uint8_t uidObjectType;
		is.read(reinterpret_cast<char*>(&uidObjectType), sizeof(uint8_t));

		switch (uidObjectType)
		{
		case TypeA::UID:
			ptrObject = std::make_shared<TypeA>(is);
			break;
		case TypeB::UID:
			ptrObject = std::make_shared<TypeB>(is);
			break;
		}
	}

	template <typename ObjectType, typename... ObjectCoreTypes>
	static void deserialize(const char* szData, ObjectType& ptrObject)
	{
		using TypeA = typename NthType<0, ObjectCoreTypes...>::type;
		using TypeB = typename NthType<1, ObjectCoreTypes...>::type;

		uint8_t uidObjectType;

		switch (szData[0])
		{
		case TypeA::UID:
			ptrObject = std::make_shared<TypeA>(szData);
			break;
		case TypeB::UID:
			ptrObject = std::make_shared<TypeB>(szData);
			break;
		}
	}
};
