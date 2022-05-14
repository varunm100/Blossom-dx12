#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <Windows.h>

#include <directx/d3dx12.h>
#include <dxgi1_6.h>
//#include <DirectXMath/DirectXMath.h>

#include <string>
#include <wrl.h>
#include <shellapi.h>

namespace d {
  using namespace Microsoft::WRL;
}
