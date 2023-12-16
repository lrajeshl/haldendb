#pragma once

enum Types
{
	//DataNode = 0,
	//IndexNode = 0
} typedef TYPES_ID;

struct TypeMarshaller 
{
private:
	//template <typename Type>
	//static std::vector<std::byte> serialize(const Type& obj) {
	//	return obj->serialize();
	//}

	//template <typename Type>
	//static void deserialize_(const std::vector<std::byte>& bytes, std::shared_ptr<Type>& type) {
	//	type = std::make_shared<Type>(bytes);
	//}
public:
	template <typename... Types>
	static std::tuple<uint8_t, const std::byte*, size_t> serialize(const std::variant<std::shared_ptr<Types>...>& myVariant) {
		std::tuple<uint8_t, const std::byte*, size_t> result;

		// Use std::visit to serialize each type
		std::visit([&result](const auto& value) {
			result = value->getSerializedBytes();
			}, myVariant);

		return result;
	}

	template <typename... Types>
	static void deserialize(uint8_t uid, std::byte* bytes, std::variant<Types...>& variant) {

		if (uid == DataNode<string, string, __COUNTER__>::UID)
		{
		std::cout << __COUNTER__;

		}
		if ( uid == DataNode<int, int, 1000>::UID)
		{
			std::cout << __COUNTER__;
		}
		if ( uid == DataNode<float, float, __COUNTER__>::UID)
		{
			std::cout << __COUNTER__;

		}

		//std::variant<std::shared_ptr<Types>...> _t;
			
		// Use std::visit to deserialize each type in the variant
		std::visit([&bytes](auto& value) {
			value.instantiateSelf(bytes);
			}, variant);
	}
private:
	///*template <typename Type>
	//std::vector<std::byte> serialize(const Type& obj) {
	//	const std::byte* dataPtr = reinterpret_cast<const std::byte*>(&obj);
	//	return { dataPtr, dataPtr + sizeof(T) };
	//}*/
	//template <typename T>
	//std::string operator()(const T& value) const {
	//	return serialize(value);
	//}

	//template <typename >
	//std::string operator()(const T& value) const {
	//	return serialize(value);
	//}
};
