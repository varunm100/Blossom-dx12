#pragma once

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include <spdlog/spdlog.h>
#include "comdef.h"

template <typename T, size_t Size>
char(*count_of_helper(T(&_arr)[Size]))[Size];

#define COUNT_OF(arr) (sizeof(*count_of_helper(arr)) + 0)

#define info_log(msg, ...) spdlog::info(msg, ##__VA_ARGS__);
#define err_log(msg, ...) spdlog::error(msg, ##__VA_ARGS__);
#define warn_log(msg, ...) spdlog::warn(msg, ##__VA_ARGS__);

#define DX_CHECK(hr)                                     \
  do {                                                   \
    if (FAILED(hr)) {                                    \
      _com_error err(hr);                                \
      err_log("File '{}', line {}", __FILE__, __LINE__); \
      err_log(L"Error {}", err.ErrorMessage());          \
      assert(0);                                         \
    }                                                    \
  } while (0)                                            \

#define assert_log(condition, msg) \
  do {				                     \
    if (!(condition)) {		         \
      err_log(msg);		             \
      assert(0);		               \
    }				                       \
  } while (0)			                 \
