#pragma once

#include "d/Pipeline.h"
#include "d/Resource.h"
#include "d/Types.h"
#include "d/stdafx.h"

namespace d {

struct DirectDrawInfo {
  std::optional<u32> start_index;
  u32 index_count;
  std::optional<u32> start_vertex;
  std::optional<Resource<Buffer>> ibo;
  ByteSpan push_constants;
};

struct DrawDirectsInfo {
  D3D12_CPU_DESCRIPTOR_HANDLE output;
  std::initializer_list<DirectDrawInfo> draw_infos;
  Pipeline &pl;
};

struct CommandList {
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> handle;
  std::vector<D3D12_TEXTURE_BARRIER> texture_barrier_buffer;
  std::vector<D3D12_BUFFER_BARRIER> buffer_barrier_buffer;

  CommandList() = default;
  ~CommandList() = default;

  auto record() -> CommandList &;
  auto finish() -> CommandList &;

  auto copy_buffer_region(Resource<Buffer> src, Resource<Buffer> dst,
                          usize size, u32 src_offset = 0, u32 dst_offset = 0)
      -> CommandList &;

  auto push_transition(Resource<D2> texture, D3D12_BARRIER_LAYOUT after)
      -> CommandList &;
  auto push_transition(Resource<Buffer> buffer, D3D12_BARRIER_LAYOUT after)
      -> CommandList &;

  auto transition() -> CommandList &;
  auto draw_directs(const DrawDirectsInfo &draw_infos) -> CommandList &;
};

} // namespace d