#pragma once
#include <cstdint>
#include <cstddef>
#include <span>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;

struct ByteSpan : std::span<const std::byte> {
	template <typename T>
	ByteSpan(const std::vector<T>& t)
		: std::span<const std::byte>(std::as_bytes(std::span{ t })) {}

	template <typename T>
	ByteSpan(const T* ptr, u32 num_elements)
		: std::span<const std::byte>(std::as_bytes(std::span{ ptr, num_elements })) {}

	template <typename T>
		requires std::is_trivially_copyable_v<T> ByteSpan(const T& t)
	: std::span<const std::byte>(std::as_bytes(std::span{ &t, 1 })) {}

	template <typename T>
		requires std::is_trivially_copyable_v<T> ByteSpan(
	std::span<const T> t)
		: std::span<const std::byte>(std::as_bytes(t)) {}

	template <typename T>
		requires std::is_trivially_copyable_v<T> ByteSpan(
	std::span<T> t)
		: std::span<const std::byte>(std::as_bytes(t)) {}
};
