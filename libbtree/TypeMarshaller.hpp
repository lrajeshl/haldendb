#pragma once
#include <variant>
#include <typeinfo>

#include "DataNode.hpp"
#include "IndexNode.hpp"
#include "VariadicNthType.h"
enum Types
{
	//DataNode = 0,
	//IndexNode = 0
} typedef TYPES_ID;

struct TypeMarshaller 
{
public:
	template <typename... ObjectCoreTypes>
	static const char* serialize(const std::variant<std::shared_ptr<ObjectCoreTypes>...>& myVariant, uint8_t& uidObjectType, size_t& nDataSize)
	{
		const char* szData = nullptr;

		std::visit([&szData, &uidObjectType, &nDataSize](const auto& value) {
			szData = value->getSerializedBytes(uidObjectType, nDataSize);
			}, myVariant);

		return szData;
	}

	template <typename... ObjectCoreTypes, typename ObjectType>
	static void deserialize(char* szData, std::shared_ptr<ObjectType>& ptrObject)
	{
		using TypeA = typename NthType<0, ObjectCoreTypes...>::type;
		using TypeB = typename NthType<1, ObjectCoreTypes...>::type;

		switch (szData[0])
		{
		case TypeA::UID:
			ptrObject = std::make_shared<ObjectType>(std::make_shared<TypeA>(szData));
			break;
		case TypeB::UID:
			ptrObject = std::make_shared<ObjectType>(std::make_shared<TypeA>(szData));
			break;
		}
	}
};
