#pragma once

#include "d/stdafx.h"
#include "d/Swapchain.h"

#include <GLFW/glfw3.h>
#include <d/D3D12MemAlloc.h>

#include "d/AssetLibrary.h"
#include "d/Queue.h"
#include "d/Resource.h"

namespace d {
struct DescHeap {
  ComPtr<ID3D12DescriptorHeap> heap;
  D3D12_CPU_DESCRIPTOR_HANDLE start;
  D3D12_CPU_DESCRIPTOR_HANDLE end;
  usize size{0};
  u32 stride;

  auto init(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_desc) -> void;

  auto push_back(const ResourceViewInfo &info) -> u32;
  auto push_back_get_handle(const ResourceViewInfo &info)
      -> D3D12_CPU_DESCRIPTOR_HANDLE;
  auto get_index_of(D3D12_CPU_DESCRIPTOR_HANDLE handle) -> u32;
};

struct DescriptorStorage {
  DescHeap render_target_heap;
  DescHeap depth_stencil_heap;
  DescHeap bindable_desc_heap;
  DescHeap push_constant_heap;

  void init(const u32 num_desc);
};

struct ResourceState {
  D3D12_RESOURCE_STATES state{D3D12_RESOURCE_STATE_COMMON};
};

struct ResourceLibrary {
  std::vector<ResourceState> resource_states;
  std::vector<ComPtr<ID3D12Resource>> resources;
  std::vector<ComPtr<D3D12MA::Allocation>> allocations;

  std::unordered_map<BufferViewInfo, D3D12_CPU_DESCRIPTOR_HANDLE>
      buffer_view_cache;
  std::unordered_map<TextureViewInfo, D3D12_CPU_DESCRIPTOR_HANDLE>
      texture_view_cache;

  DescriptorStorage storage;

  [[nodiscard]] auto get_resource_index(const ResourceViewInfo &info) -> u32;
  [[nodiscard]] auto get_desc_handle(const ResourceViewInfo &info)
      -> D3D12_CPU_DESCRIPTOR_HANDLE;
};

struct Context {
  ComPtr<ID3D12Device5> device;
  ComPtr<IDXGIFactory5> factory;

  Queue general_queue;
  // TODO: add multiple queue support
#ifdef _DEBUG
  ComPtr<ID3D12Debug> debug_interface;
  ComPtr<ID3D12Debug1> debug_interface1;
#endif

  ComPtr<ID3D12DescriptorHeap> desc_heap_rtv;

  CommandList main_command_list;

  Queue async_transfer_q;
  Queue async_compute_q;
  Queue general_q;

  ComPtr<D3D12MA::Allocator> allocator;
  Swapchain swap_chain;
  AssetLibrary library;
  ResourceLibrary res_lib;

  Context() = default;

  ~Context() = default;

  void init(GLFWwindow *window, u32 sc_count);

  [[nodiscard]] std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, CommandList &>
  BeginRendering();

  void EndRendering();

  [[nodiscard]] Resource<Buffer> create_buffer(BufferCreateInfo &&create_info);

  [[nodiscard]] Resource<D2> create_texture_2d(
      TextureCreateInfo &&texture_info);

  u32 RegisterResource(const ComPtr<ID3D12Resource> &resource,
                       const ComPtr<D3D12MA::Allocation> &allocation);

  D3D12_RESOURCE_BARRIER
  get_transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES state_before,
                 D3D12_RESOURCE_STATES state_after) const;
};

extern Context c;

template <ResourceC T>
inline ID3D12Resource *get_native_res(Resource<T> handle) {
  u32 index = static_cast<u32>(handle);
  return c.res_lib.resources[index].Get();
};

inline ID3D12Resource *get_native_res(Handle handle) {
  return c.res_lib.resources[handle].Get();
}

void InitContext(GLFWwindow *window, u32 sc_count);

}  // namespace d