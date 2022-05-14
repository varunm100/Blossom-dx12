#pragma once


#define SPDLOG_WCHAR_TO_UTF8_SUPPORT

#include <spdlog/spdlog.h>
#include "comdef.h"

//inline void DX_CHECK(const HRESULT hr) {
//  if (FAILED(hr)) {
//    spdlog::error("error: {}", err.ErrorMessage());
//    throw std::exception();
//  }
//}

#define DX_CHECK(hr)                                \
  do {                                              \
    if (FAILED(hr)) {                               \
      _com_error err(hr);                           \
      spdlog::error("File '{}', line {}", \
      __FILE__, __LINE__);      \
      spdlog::error(L"Error {}", err.ErrorMessage()); \
      assert(0);     \
    }                                               \
  } while (0)                                       \
