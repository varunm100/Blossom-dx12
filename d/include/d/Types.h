#pragma once
#include <stdint.h>

#include <span>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;

//using ByteSpan = std::span<const std::byte>

struct ByteSpan : public std::span<const std::byte> {
  template <typename T>
   ByteSpan(const std::vector<T>& t)
      : std::span<const std::byte>(std::as_bytes(std::span{t})) {}

  template <typename T>
  requires std::is_trivially_copyable_v<T> ByteSpan(const T& t)
      : std::span<const std::byte>(std::as_bytes(std::span{&t, 1})) {}

  template <typename T>
  requires std::is_trivially_copyable_v<T> ByteSpan(
      std::span<const T> t)
      : std::span<const std::byte>(std::as_bytes(t)) {}

  template <typename T>
  requires std::is_trivially_copyable_v<T> ByteSpan(
      std::span<T> t)
      : std::span<const std::byte>(std::as_bytes(t)) {}
};
