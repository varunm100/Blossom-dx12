#pragma once
#include "stdafx.h"
#include "d/Types.h"
#include "d/Resource.h"
#include <vector>

namespace d {
  struct Swapchain {
    ComPtr<IDXGISwapChain1> swapchain;
    std::vector<ComPtr<ID3D12Resource>> image_buffers;
//    std::vector<Resource<D2>> images;
    DXGI_FORMAT format;
    u32 width{0};
    u32 height{0};
    u32 image_index{0};
  };
}