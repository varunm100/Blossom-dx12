#include "d/Resource.h"

#include <DirectXTex.h>

#include <filesystem>

#include "d/Context.h"
#include "d/Logging.h"

namespace d {

u32 Context::RegisterResource(const ComPtr<ID3D12Resource> &resource,
                              const ComPtr<D3D12MA::Allocation> &allocation) {
  bool found_spot = false;
  usize spot = 0;
  for (const auto &res : res_lib.resources) {
    if (res == nullptr) {
      found_spot = true;
      break;
    }
    ++spot;
  }
  u32 handle = found_spot ? static_cast<u32>(spot)
                          : static_cast<u32>(res_lib.resources.size());

  const auto default_state =
      ResourceState{.state = D3D12_RESOURCE_STATE_COMMON};

  if (!found_spot)
    res_lib.resources.push_back(resource),
        res_lib.allocations.push_back(allocation),
        res_lib.resource_states.push_back(default_state);
  else
    res_lib.resources[spot] = resource, res_lib.allocations[spot] = allocation,
    res_lib.resource_states[spot] = default_state;

  return handle;
}

Resource<Buffer> Context::create_buffer(BufferCreateInfo &&create_info) {
  auto resource_desc = D3D12_RESOURCE_DESC{
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = create_info.size,
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .Format = DXGI_FORMAT_UNKNOWN,
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      .Flags = D3D12_RESOURCE_FLAG_NONE,
  };
  resource_desc.SampleDesc.Count = 1;
  resource_desc.SampleDesc.Quality = 0;

  D3D12MA::ALLOCATION_DESC allocation_desc = {};
  D3D12_RESOURCE_STATES state;
  switch (create_info.usage) {
  case MemoryUsage::GPU:
    allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    state = D3D12_RESOURCE_STATE_COPY_DEST;
    break;
  case MemoryUsage::Mappable:
    allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    state = D3D12_RESOURCE_STATE_GENERIC_READ;
    break;
  case MemoryUsage::CPU_Readable:
    allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
    state = D3D12_RESOURCE_STATE_GENERIC_READ;
    break;
  }

  ComPtr<D3D12MA::Allocation> allocation;
  ComPtr<ID3D12Resource> resource;

  DX_CHECK(allocator->CreateResource(&allocation_desc, &resource_desc, state,
                                     nullptr, &allocation,
                                     IID_PPV_ARGS(&resource)));

  const u32 handle = RegisterResource(resource, allocation);

  return Resource<Buffer>(handle);
}

Resource<D2> Context::create_texture_2d(TextureCreateInfo &&texture_info) {
  auto desc = D3D12_RESOURCE_DESC{};
  D3D12_RESOURCE_FLAGS res_flags = D3D12_RESOURCE_FLAG_NONE;
  if (texture_info.usage == TextureUsage::RENDER_TARGET) {
    res_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  } else if (texture_info.usage == TextureUsage::DEPTH_STENCIL) {
    res_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  }
  if (texture_info.usage == TextureUsage::SHADER_READ_WRITE_ATOMIC) {
    res_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }
  // TODO: mip levels hard coded right now :(
  desc = CD3DX12_RESOURCE_DESC::Tex2D(
      texture_info.format, texture_info.extent.width,
      texture_info.extent.height, texture_info.extent.array_size, 0, 1, 0,
      res_flags);

  ComPtr<D3D12MA::Allocation> allocation;
  ComPtr<ID3D12Resource> resource;

  D3D12MA::ALLOCATION_DESC allocation_desc = {};
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;

  DX_CHECK(allocator->CreateResource(&allocation_desc, &desc, state, nullptr,
                                     &allocation, IID_PPV_ARGS(&resource)));

  u32 handle = RegisterResource(resource, allocation);

  return Resource<D2>(handle);
}

std::variant<D3D12_SHADER_RESOURCE_VIEW_DESC, D3D12_UNORDERED_ACCESS_VIEW_DESC>
BufferViewInfo::get_native_view() const {
  if (buffer_usage == BufferUsage::SHADER_READ) {
    return D3D12_SHADER_RESOURCE_VIEW_DESC{
        .Format = stride ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS,
        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Buffer =
            D3D12_BUFFER_SRV{
                .FirstElement = first_element,
                .NumElements = num_elements,
                .StructureByteStride = stride,
                .Flags = stride ? D3D12_BUFFER_SRV_FLAG_NONE
                                : D3D12_BUFFER_SRV_FLAG_RAW,
            },
    };
  } else { // atomic read_write
    return D3D12_UNORDERED_ACCESS_VIEW_DESC{
        .Format = stride ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer =
            D3D12_BUFFER_UAV{
                .FirstElement = first_element,
                .NumElements = num_elements,
                .StructureByteStride = stride,
                .CounterOffsetInBytes = 0u, // TODO: no UAV counter support
                .Flags = stride ? D3D12_BUFFER_UAV_FLAG_NONE
                                : D3D12_BUFFER_UAV_FLAG_RAW,
            },
    };
  }
}

std::variant<D3D12_SHADER_RESOURCE_VIEW_DESC, D3D12_UNORDERED_ACCESS_VIEW_DESC,
             D3D12_RENDER_TARGET_VIEW_DESC, D3D12_DEPTH_STENCIL_VIEW_DESC>
TextureViewInfo::get_native_view() const {
  if (type == ResourceType::D2) {
    if (texture_usage == TextureUsage::SHADER_READ) {
      return D3D12_SHADER_RESOURCE_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
          .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
          .Texture2D =
              D3D12_TEX2D_SRV{
                  .MostDetailedMip = highest_quality_mip,
                  .MipLevels = num_mips,
                  .PlaneSlice = 0,
                  .ResourceMinLODClamp = min_mip_clamp,
              },
      };
    } else if (texture_usage == TextureUsage::SHADER_READ_WRITE_ATOMIC) {
      return D3D12_UNORDERED_ACCESS_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
          .Texture2D =
              D3D12_TEX2D_UAV{
                  .MipSlice = mip_slice,
                  .PlaneSlice = 0,
              },
      };
    } else if (texture_usage == TextureUsage::RENDER_TARGET) {
      return D3D12_RENDER_TARGET_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
          .Texture2D =
              D3D12_TEX2D_RTV{
                  .MipSlice = mip_slice,
                  .PlaneSlice = 0,
              },
      };
    } else { // DEPTH TEXTURE
      return D3D12_DEPTH_STENCIL_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
          .Flags = D3D12_DSV_FLAG_NONE,
          .Texture2D =
              D3D12_TEX2D_DSV{
                  .MipSlice = mip_slice,
              },
      };
    }
  } else if (type == ResourceType::D2Array) {
    if (texture_usage == TextureUsage::SHADER_READ) {
      return D3D12_SHADER_RESOURCE_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY,
          .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
          .Texture2DArray =
              D3D12_TEX2D_ARRAY_SRV{
                  .MostDetailedMip = highest_quality_mip,
                  .MipLevels = num_mips,
                  .FirstArraySlice = first_array_depth_slice,
                  .ArraySize = num_slices,
                  .PlaneSlice = 0,
                  .ResourceMinLODClamp = min_mip_clamp,
              },
      };
    } else if (texture_usage ==
               TextureUsage::SHADER_READ_WRITE_ATOMIC) { // atomic read_write
      return D3D12_UNORDERED_ACCESS_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY,
          .Texture2DArray =
              D3D12_TEX2D_ARRAY_UAV{
                  .MipSlice = mip_slice,
                  .FirstArraySlice = first_array_depth_slice,
                  .ArraySize = num_slices,
                  .PlaneSlice = 0,
              },
      };
    } else if (texture_usage == TextureUsage::RENDER_TARGET) {
      return D3D12_RENDER_TARGET_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY,
          .Texture2DArray =
              D3D12_TEX2D_ARRAY_RTV{
                  .MipSlice = mip_slice,
                  .FirstArraySlice = first_array_depth_slice,
                  .ArraySize = num_slices,
                  .PlaneSlice = 0,
              },
      };
    } else { // DEPTH TEXTURE
      return D3D12_DEPTH_STENCIL_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
          .Flags = D3D12_DSV_FLAG_NONE,
          .Texture2DArray =
              D3D12_TEX2D_ARRAY_DSV{
                  .MipSlice = mip_slice,
                  .FirstArraySlice = first_array_depth_slice,
                  .ArraySize = num_slices,
              },
      };
    }
  } else if (type == ResourceType::D3) {
    if (texture_usage == TextureUsage::SHADER_READ) {
      return D3D12_SHADER_RESOURCE_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D,
          .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
          .Texture3D =
              D3D12_TEX3D_SRV{
                  .MostDetailedMip = highest_quality_mip,
                  .MipLevels = num_mips,
                  .ResourceMinLODClamp = min_mip_clamp,
              },
      };
    } else if (texture_usage ==
               TextureUsage::SHADER_READ_WRITE_ATOMIC) { // atomic read_write
      return D3D12_UNORDERED_ACCESS_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D,
          .Texture3D =
              D3D12_TEX3D_UAV{
                  .MipSlice = mip_slice,
                  .FirstWSlice = first_array_depth_slice,
                  .WSize = num_slices,
              },
      };
    } else if (texture_usage == TextureUsage::RENDER_TARGET) {
      return D3D12_RENDER_TARGET_VIEW_DESC{
          .Format = format,
          .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D,
          .Texture3D =
              D3D12_TEX3D_RTV{
                  .MipSlice = mip_slice,
                  .FirstWSlice = first_array_depth_slice,
                  .WSize = num_slices,
              },
      };
    } else { // DEPTH TEXTURE
      spdlog::error("Cannot create depth/stencil view from 3D texture");
      throw std::exception("Cannot create depth/stencil view from 3D texture");
    }
  } else {
    spdlog::error("Unexpected Resource Type");
    throw std::exception("Unexpected Resource Type");
  }
}

[[nodiscard]] auto Resource<D2>::rtv_view(std::optional<u32> mip_slice) const
    -> ResourceViewInfo {
  return ResourceViewInfo{
      .views =
          {
              .texture_view =
                  {
                      .resource_handle = static_cast<Handle>(handle),
                      .format = get_native_res(static_cast<Handle>(handle))
                                    ->GetDesc()
                                    .Format,
                      .texture_usage = TextureUsage::RENDER_TARGET,
                      .mip_slice = mip_slice.value_or(0u),
                      .type = ResourceType::D2,
                  },
          },
      .type = ResourceType::D2,
  };
}

[[nodiscard]] auto ResourceViewInfo::desc_index() const -> u32 {
  auto *heap = &c.res_lib.storage.bindable_desc_heap;
  auto &lib = c.res_lib;
  if (type == ResourceType::Buffer) {
    return lib.buffer_view_cache.contains(views.buffer_view)
               ? heap->get_index_of(lib.buffer_view_cache[views.buffer_view])
               : heap->push_back(*this);
  }
  const auto usage = views.texture_view.texture_usage;
  if (usage == TextureUsage::RENDER_TARGET) {
    heap = &c.res_lib.storage.render_target_heap;
  } else if (usage == TextureUsage::DEPTH_STENCIL) {
    heap = &c.res_lib.storage.depth_stencil_heap;
  }
  return lib.texture_view_cache.contains(views.texture_view)
             ? heap->get_index_of(lib.texture_view_cache[views.texture_view])
             : heap->push_back(*this);
}

[[nodiscard]] auto ResourceViewInfo::desc_handle() const
    -> D3D12_CPU_DESCRIPTOR_HANDLE {
  auto *heap = &c.res_lib.storage.bindable_desc_heap;
  auto &lib = c.res_lib;
  if (type == ResourceType::Buffer) {
    return lib.buffer_view_cache.contains(views.buffer_view)
               ? lib.buffer_view_cache[views.buffer_view]
               : heap->push_back_get_handle(*this);
  }
  const auto usage = views.texture_view.texture_usage;
  if (usage == TextureUsage::RENDER_TARGET) {
    heap = &c.res_lib.storage.render_target_heap;
  } else if (usage == TextureUsage::DEPTH_STENCIL) {
    heap = &c.res_lib.storage.depth_stencil_heap;
  }
  return lib.texture_view_cache.contains(views.texture_view)
             ? lib.texture_view_cache[views.texture_view]
             : heap->push_back_get_handle(*this);
}

auto Resource<Buffer>::map_and_copy(ByteSpan data, usize offset) const -> void {
  void *mapped;
  DX_CHECK(
      d::get_native_res(static_cast<u32>(handle))->Map(0, nullptr, &mapped));
  memcpy(static_cast<char*>(mapped) + offset, reinterpret_cast<void*>(data.data()), data.size());
  d::get_native_res(static_cast<u32>(handle))->Unmap(0, nullptr);
}

[[nodiscard]] auto Resource<Buffer>::ibo_view(std::optional<u32> index_offset,
                                              u32 num_indices) const
    -> D3D12_INDEX_BUFFER_VIEW {
  return D3D12_INDEX_BUFFER_VIEW{
      .BufferLocation = get_native_res(*this)->GetGPUVirtualAddress() +
                        index_offset.value_or(0u) * sizeof(u32),
      .SizeInBytes = num_indices * static_cast<UINT>(sizeof(u32)),
      .Format = DXGI_FORMAT_R32_UINT,
  };
}

[[nodiscard]] Resource<D2> util::create_texture_from_file(const char *path) {
  using namespace DirectX;
  const std::filesystem::path texture_path(path);
  TexMetadata metadata;
  ScratchImage scratch;

  if (!std::filesystem::exists(texture_path)) {
    spdlog::error("Cannot find texture file: {} ", path);
    throw std::exception("File not found.");
  }
  if (texture_path.extension() == ".dds") {
    DX_CHECK(LoadFromDDSFile(texture_path.c_str(), DDS_FLAGS_FORCE_RGB,
                             &metadata, scratch));
  } else if (texture_path.extension() == ".hdr") {
    DX_CHECK(LoadFromHDRFile(texture_path.c_str(), &metadata, scratch));
  } else if (texture_path.extension() == ".tga") {
    DX_CHECK(LoadFromTGAFile(texture_path.c_str(), &metadata, scratch));
  } else {
    DX_CHECK(LoadFromWICFile(texture_path.c_str(), WIC_FLAGS_FORCE_RGB,
                             &metadata, scratch));
  }
  Resource<D2> res = c.create_texture_2d(TextureCreateInfo{
      .format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .dim = TextureDimension::D2,
      .extent = TextureExtent{.width = static_cast<u32>(metadata.width),
                              .height = static_cast<u32>(metadata.height)},
      .usage = TextureUsage::SHADER_READ,
  });
  std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratch.GetImageCount());
  const Image *pImages = scratch.GetImages();
  for (int i = 0; i < scratch.GetImageCount(); ++i) {
    auto &subresource = subresources[i];
    subresource.RowPitch = pImages[i].rowPitch;
    subresource.SlicePitch = pImages[i].slicePitch;
    subresource.pData = pImages[i].pixels;
  }
  UINT64 requiredSize = GetRequiredIntermediateSize(
      get_native_res(res), 0, static_cast<u32>(subresources.size()));

  // copy buffer
}
} // namespace d
