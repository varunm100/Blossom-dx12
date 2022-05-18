#define GLFW_EXPOSE_NATIVE_WIN32

#include "d/Context.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <variant>

#include "d/Logging.h"

namespace d {

Context c;

void Context::init(GLFWwindow *window, u32 sc_count) {
  DX_CHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
  debug_interface->EnableDebugLayer();
  DX_CHECK(debug_interface->QueryInterface(IID_PPV_ARGS(&debug_interface1)));
  debug_interface1->SetEnableGPUBasedValidation(true);
  debug_interface1->SetEnableSynchronizedCommandQueueValidation(true);

  UINT factory_flags = 0u;
#ifdef _DEBUG
  factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  DX_CHECK(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)));

  // pick device
  {
    SIZE_T max_v_ram = 0u;

    ComPtr<IDXGIAdapter1> adapter;
    u32 suitable_adapter = 0u;
    for (UINT i = 0;
         factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
      DXGI_ADAPTER_DESC1 adapter_desc;
      adapter->GetDesc1(&adapter_desc);

      if ((adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
          SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                      __uuidof(ID3D12Device), nullptr)) &&
          adapter_desc.DedicatedVideoMemory > max_v_ram) {
        max_v_ram = adapter_desc.DedicatedVideoMemory;
        suitable_adapter = i;
      }
      spdlog::info(L"Adapter {}: {}", i, adapter_desc.Description,
                   adapter_desc.DedicatedVideoMemory);
    }
    spdlog::info("Picked device: {}", suitable_adapter);
    factory->EnumAdapters1(suitable_adapter, &adapter);
    DX_CHECK(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                               IID_PPV_ARGS(&device)));

    auto allocator_desc = D3D12MA::ALLOCATOR_DESC{
        .pDevice = device.Get(),
        .pAdapter = adapter.Get(),
    };

    DX_CHECK(D3D12MA::CreateAllocator(&allocator_desc, &allocator));
  }

  // create command general command queue
  general_queue.init(QueueType::GENERAL);
  main_command_list = general_queue.get_command_list();

  int width, height;
  glfwGetWindowSize(window, &width, &height);
  swap_chain.image_index = 0;
  swap_chain.width = width;
  swap_chain.height = height;
  swap_chain.format = DXGI_FORMAT_R8G8B8A8_UNORM;
  DXGI_SWAP_CHAIN_DESC1 sc_desc = {
      .Width = swap_chain.width,
      .Height = swap_chain.height,
      .Format = swap_chain.format,
      .Stereo = FALSE,
      .SampleDesc = {1, 0},
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = sc_count,
      .Scaling = DXGI_SCALING_STRETCH,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
      .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING,
  };

  // create swap_chain and RTV ( render target views);
  HWND win_handle = glfwGetWin32Window(window);
  DX_CHECK(factory->CreateSwapChainForHwnd(general_queue.handle.Get(),
                                           win_handle, &sc_desc, nullptr,
                                           nullptr, &swap_chain.swapchain));
  DX_CHECK(factory->MakeWindowAssociation(win_handle, DXGI_MWA_NO_ALT_ENTER));

  general_q.init(QueueType::GENERAL);
  async_transfer_q.init(QueueType::ASYNC_TRANSFER);
  async_compute_q.init(QueueType::ASYNC_COMPUTE);

  library.init();

  c.res_lib.storage.init(100);

  swap_chain.images.reserve(sc_count);
  for (u32 i = 0; i < sc_count; ++i) {
    ComPtr<ID3D12Resource> image;
    ComPtr<D3D12MA::Allocation> empty_allocation;
    
    DX_CHECK(swap_chain.swapchain->GetBuffer(i, IID_PPV_ARGS(&image)));
    auto res = Resource<D2>(RegisterResource(image, empty_allocation));
    swap_chain.images.push_back(res);

    // initialize cache
    auto image_view_handle = swap_chain.images[i].rtv_view({})
  																																		 .desc_handle();
  }

  //D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {};
  //DX_CHECK(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12,
  //                                     &options12, sizeof(options12)));
  //if (options12.EnhancedBarriersSupported) {
  //  spdlog::error("Enhanced Barriers not supported, crashing...");
  //  assert(0);
  //} else {
  //  spdlog::info("Enhanced Barriers supported :)");
  //}

}

[[nodiscard]] std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, CommandList &>
Context::BeginRendering() {
  // start rendering
  main_command_list.record();

  auto screen_viewport = D3D12_VIEWPORT{
      .TopLeftX = 0.,
      .TopLeftY = 0.,
      .Width = static_cast<float>(swap_chain.width),
      .Height = static_cast<float>(swap_chain.height),
      .MinDepth = 0.f,
      .MaxDepth = 1.f,
  };
  auto screen_scissor =
      D3D12_RECT{.left = 0,
                 .top = 0,
                 .right = static_cast<long>(swap_chain.width),
                 .bottom = static_cast<long>(swap_chain.height)};
  main_command_list.handle->RSSetViewports(1, &screen_viewport);
  main_command_list.handle->RSSetScissorRects(1, &screen_scissor);

  const u32 &image_index = swap_chain.image_index;

  // create barrier and get render target handle
  const D3D12_RESOURCE_BARRIER rd_barrier = get_transition(get_native_res(swap_chain.images[image_index]),
                              get_res_state(swap_chain.images[image_index]).state,
                              D3D12_RESOURCE_STATE_RENDER_TARGET);
  get_res_state(swap_chain.images[image_index]).state =
      D3D12_RESOURCE_STATE_RENDER_TARGET;
  main_command_list.handle->ResourceBarrier(1, &rd_barrier);

  DX_CHECK(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

  return std::make_pair(swap_chain.images[image_index].rtv_view({}).desc_handle(), std::ref(main_command_list));
}

D3D12_RESOURCE_BARRIER Context::get_transition(
    ID3D12Resource *resource, D3D12_RESOURCE_STATES state_before,
    D3D12_RESOURCE_STATES state_after) const {
  auto rd_barrier = D3D12_RESOURCE_BARRIER{
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
  };
  rd_barrier.Transition.pResource = resource;
  rd_barrier.Transition.StateBefore = state_before;
  rd_barrier.Transition.StateAfter = state_after;
  return rd_barrier;
}

void Context::EndRendering() {
  u32 &image_index = swap_chain.image_index;

  const auto rd_barrier = get_transition(get_native_res(swap_chain.images[image_index]),
                                         get_res_state(swap_chain.images[image_index]).state,
                                         D3D12_RESOURCE_STATE_PRESENT);
  get_res_state(swap_chain.images[image_index]).state =
      D3D12_RESOURCE_STATE_PRESENT;
  main_command_list.handle->ResourceBarrier(1, &rd_barrier);

  main_command_list.finish();
  general_queue.submit_lists({main_command_list});

  DX_CHECK(swap_chain.swapchain->Present(0, 0));
  image_index = (image_index + 1u) % swap_chain.images.size();

  // FLUSH COMMAND QUEUE TODO: have in_flight images
  general_queue.block_until_idle();
}

void InitContext(GLFWwindow *window, u32 sc_count) {
  c = d::Context();
  c.init(window, 3);
}

void DescriptorStorage::init(const u32 num_desc) {
  bindable_desc_heap.init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, num_desc);
  render_target_heap.init(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, num_desc);
  depth_stencil_heap.init(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, num_desc);
}

void DescHeap::init(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_desc) {
  const D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {
      .Type = type,
      .NumDescriptors = num_desc,
      .Flags = type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
                   ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                   : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
  };

  DX_CHECK(
      c.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)));
  start = heap->GetCPUDescriptorHandleForHeapStart();
  end = start;
  stride = c.device->GetDescriptorHandleIncrementSize(type);
  size = 0;
}

u32 DescHeap::get_index_of(D3D12_CPU_DESCRIPTOR_HANDLE handle) {
  return static_cast<u32>((handle.ptr - start.ptr) / stride);
}

u32 DescHeap::push_back(const ResourceViewInfo &res_info) {
  if (res_info.type == ResourceType::Buffer) {
    auto &info = res_info.views.buffer_view;
    auto view = info.get_native_view();
    if (view.index() == 0) {
      auto &v = std::get<0>(view);
      d::c.device->CreateShaderResourceView(
          get_native_res(info.resource_handle), &v, end);
    } else if (view.index() == 1) {
      auto &v = std::get<1>(view);
      // no counter
      d::c.device->CreateUnorderedAccessView(
          get_native_res(info.resource_handle), nullptr, &v, end);
    }
    c.res_lib.buffer_view_cache[res_info.views.buffer_view] = end;
    end.ptr += stride;
    ++size;
    return static_cast<u32>(size - 1);
  } else {  // assumes texture
    std::variant<D3D12_SHADER_RESOURCE_VIEW_DESC,
                 D3D12_UNORDERED_ACCESS_VIEW_DESC,
                 D3D12_RENDER_TARGET_VIEW_DESC, D3D12_DEPTH_STENCIL_VIEW_DESC>
        view = res_info.views.texture_view.get_native_view();
    auto &info = res_info.views.texture_view;
    if (view.index() == 0) {
      auto &v = std::get<0>(view);
      d::c.device->CreateShaderResourceView(
          get_native_res(info.resource_handle), &v, end);
    } else if (view.index() == 1) {
      auto &v = std::get<1>(view);
      // no counter support, TODO possibly?
      d::c.device->CreateUnorderedAccessView(
          get_native_res(info.resource_handle), nullptr, &v, end);
    } else if (view.index() == 2) {
      auto &v = std::get<2>(view);
      d::c.device->CreateRenderTargetView(get_native_res(info.resource_handle),
                                          &v, end);
    } else if (view.index() == 3) {
      auto &v = std::get<3>(view);
      d::c.device->CreateDepthStencilView(get_native_res(info.resource_handle),
                                          &v, end);
    }
    c.res_lib.texture_view_cache[res_info.views.texture_view] = end;
    end.ptr += stride;
    ++size;
    return static_cast<u32>(size - 1);
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE DescHeap::push_back_get_handle(
    const ResourceViewInfo &res_info) {
  push_back(res_info);
  D3D12_CPU_DESCRIPTOR_HANDLE _end = end;
  _end.ptr -= stride;
  return _end;
}

}  // namespace d