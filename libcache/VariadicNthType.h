#pragma once
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

// Access a type from the parameter pack at index 'Index'
template <std::size_t Index, typename... Types>
struct GetTypeAtIndex;

template <std::size_t Index, typename Head, typename... Tail>
struct GetTypeAtIndex<Index, Head, Tail...> {
	using Type = typename GetTypeAtIndex<Index - 1, Tail...>::Type;
};

template <typename Head, typename... Tail>
struct GetTypeAtIndex<0, Head, Tail...> {
	using Type = Head;
};

template <size_t N, typename... Args>
decltype(auto) getNthElement(Args&&... args) {
	static_assert(N < sizeof...(Args), "Index out of bounds");
	return std::get<N>(std::forward_as_tuple(std::forward<Args>(args)...));
}