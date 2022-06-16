#include "d/Resource.h"

#include <DirectXTex.h>

#include <filesystem>

#include "d/Context.h"
#include "d/Logging.h"

namespace d {

	u32 Context::register_resource(const ComPtr<ID3D12Resource>& resource,
		const ComPtr<D3D12MA::Allocation>& allocation, ResourceState initial_state) {
		bool found_spot = false;
		usize spot = 0;
		for (const auto& res : resource_registry.resources) {
			if (res == nullptr) {
				found_spot = true;
				break;
			}
			++spot;
		}
		const u32 handle = found_spot ? static_cast<u32>(spot)
			: static_cast<u32>(resource_registry.resources.size());


		if (!found_spot)
			resource_registry.resources.push_back(resource),
			resource_registry.allocations.push_back(allocation),
			resource_registry.resource_states.push_back(initial_state);
		else
			resource_registry.resources[spot] = resource, resource_registry.allocations[spot] = allocation,
			resource_registry.resource_states[spot] = initial_state;

		return handle;
	}

	// only release staging resources! resources that have views need to flush their view cache which is not implemented yet :)
	auto Context::release_resource(Handle handle)-> void {
		resource_registry.allocations[handle] = nullptr;
		resource_registry.resources[handle] = nullptr;
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
		}
		else { // atomic read_write
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
			}
			else if (texture_usage == TextureUsage::SHADER_READ_WRITE_ATOMIC) {
				return D3D12_UNORDERED_ACCESS_VIEW_DESC{
						.Format = format,
						.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
						.Texture2D =
								D3D12_TEX2D_UAV{
										.MipSlice = mip_slice,
										.PlaneSlice = 0,
								},
				};
			}
			else if (texture_usage == TextureUsage::RENDER_TARGET) {
				return D3D12_RENDER_TARGET_VIEW_DESC{
						.Format = format,
						.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
						.Texture2D =
								D3D12_TEX2D_RTV{
										.MipSlice = mip_slice,
										.PlaneSlice = 0,
								},
				};
			}
			else { // DEPTH TEXTURE
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
		}
		else if (type == ResourceType::D2Array) {
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
			}
			else if (texture_usage ==
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
			}
			else if (texture_usage == TextureUsage::RENDER_TARGET) {
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
			}
			else { // DEPTH TEXTURE
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
		}
		else if (type == ResourceType::D3) {
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
			}
			else if (texture_usage ==
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
			}
			else if (texture_usage == TextureUsage::RENDER_TARGET) {
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
			}
			else { // DEPTH TEXTURE
				assert_log(0, "Cannot create depth/stencil view from 3D texture");
				return D3D12_SHADER_RESOURCE_VIEW_DESC{};
			}
		}
		else {
			assert_log(0, "Unexpected Resource Type");
			return D3D12_SHADER_RESOURCE_VIEW_DESC{};
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

	[[nodiscard]] auto AccelerationStructureViewInfo::desc_index() const->u32 {
		auto& heap = c.resource_registry.storage.bindable_desc_heap;
		auto& lib = c.resource_registry;
		return lib.acceleration_structure_cache.contains(*this)
			? heap.get_index_of(lib.acceleration_structure_cache[*this])
			: heap.push_back(*this);
	};

	[[nodiscard]] auto ResourceViewInfo::desc_index() const -> u32 {
		auto* heap = &c.resource_registry.storage.bindable_desc_heap;
		auto& lib = c.resource_registry;
		if (type == ResourceType::Buffer) {
			return lib.buffer_view_cache.contains(views.buffer_view)
				? heap->get_index_of(lib.buffer_view_cache[views.buffer_view])
				: heap->push_back(*this);
		}
		const auto usage = views.texture_view.texture_usage;
		if (usage == TextureUsage::RENDER_TARGET) {
			heap = &c.resource_registry.storage.render_target_heap;
		}
		else if (usage == TextureUsage::DEPTH_STENCIL) {
			heap = &c.resource_registry.storage.depth_stencil_heap;
		}
		return lib.texture_view_cache.contains(views.texture_view)
			? heap->get_index_of(lib.texture_view_cache[views.texture_view])
			: heap->push_back(*this);
	}

	[[nodiscard]] auto ResourceViewInfo::desc_handle() const
		-> D3D12_CPU_DESCRIPTOR_HANDLE {
		auto* heap = &c.resource_registry.storage.bindable_desc_heap;
		auto& lib = c.resource_registry;
		if (type == ResourceType::Buffer) {
			return lib.buffer_view_cache.contains(views.buffer_view)
				? lib.buffer_view_cache[views.buffer_view]
				: heap->push_back_get_handle(*this);
		}
		const auto usage = views.texture_view.texture_usage;
		if (usage == TextureUsage::RENDER_TARGET) {
			heap = &c.resource_registry.storage.render_target_heap;
		}
		else if (usage == TextureUsage::DEPTH_STENCIL) {
			heap = &c.resource_registry.storage.depth_stencil_heap;
		}
		return lib.texture_view_cache.contains(views.texture_view)
			? lib.texture_view_cache[views.texture_view]
			: heap->push_back_get_handle(*this);
	}

	[[nodiscard]] auto TextureExtent::full_swap_chain() -> TextureExtent {
		return TextureExtent{
			.width = c.swap_chain.width,
			.height = c.swap_chain.height,
		};
	}

	[[nodiscard]] auto Resource<AccelStructure>::gpu_addr() const -> D3D12_GPU_VIRTUAL_ADDRESS {
		return get_native_res(*this)->GetGPUVirtualAddress();
	}

	Resource<Buffer> Resource<Buffer>::operator>>(const std::string_view& name) const {
		c.resource_registry.named_resource_map[name] = static_cast<u32>(this->handle);
		return *this;
	}

	auto Resource<Buffer>::map_and_copy(ByteSpan data, usize offset) const -> void {
		void* mapped;
		auto res = d::get_native_res(static_cast<u32>(handle));
		DX_CHECK(res->Map(0, nullptr, &mapped));
		memcpy(static_cast<char*>(mapped) + offset, data.data(), data.size());
		res->Unmap(0, nullptr);
	}

	[[nodiscard]] auto Resource<Buffer>::gpu_addr() const -> D3D12_GPU_VIRTUAL_ADDRESS {
		return get_native_res(*this)->GetGPUVirtualAddress();
	}

	[[nodiscard]] auto Resource<Buffer>::gpu_strided_addr(usize stride) const -> D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE {
		return D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE{
			.StartAddress = get_native_res(*this)->GetGPUVirtualAddress(),
			.StrideInBytes = stride,
		};
	}

	[[nodiscard]] auto Resource<Buffer>::gpu_addr_range(usize size, usize start_offset) const -> D3D12_GPU_VIRTUAL_ADDRESS_RANGE {
		return D3D12_GPU_VIRTUAL_ADDRESS_RANGE{
			.StartAddress = gpu_addr() + start_offset,
			.SizeInBytes = size,
		};
	}

	[[nodiscard]] auto Resource<Buffer>::gpu_strided_addr_range(usize stride, usize size, usize start_offset) const -> D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE {
		return D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{
			.StartAddress = gpu_addr() + start_offset,
			.SizeInBytes = size,
			.StrideInBytes = stride,
		};
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
} // namespace d
