#pragma once

#include "d/stdafx.h"
#include "d/Resource.h"
#include "d/Pipeline.h"
#include "d/Types.h"

namespace d {

  struct DirectDrawInfo {
    u32 index_count;
    u32 start_index;
    ByteSpan push_constants;
    std::optional<u32> constant_offset{0};
  };

  struct DrawDirectsInfo {
    D3D12_CPU_DESCRIPTOR_HANDLE output;
    std::initializer_list<DirectDrawInfo> draw_infos;
    Pipeline& pl;
  };

  struct CommandList {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> handle;

    CommandList()  = default;
    ~CommandList()  = default;

    auto record() -> CommandList&;
    auto finish() -> CommandList&;

    auto copy_buffer_region(Resource<Buffer> src, Resource<Buffer> dst, usize size, u32 src_offset=0, u32 dst_offset=0) -> CommandList&;
    auto draw_directs(const DrawDirectsInfo& draw_infos) -> CommandList&;
  };

}