#include "d/Resource.h"

#include <DirectXTex.h>

#include <filesystem>

#include "d/Context.h"
#include "d/Logging.h"

namespace d {

	u32 Context::RegisterResource(const ComPtr<ID3D12Resource>& resource,
		const ComPtr<D3D12MA::Allocation>& allocation) {
		bool found_spot = false;
		usize spot = 0;
		for (const auto& res : res_lib.resources) {
			if (res == nullptr) {
				found_spot = true;
				break;
			}
			++spot;
		}
		u32 handle = found_spot ? static_cast<u32>(spot)
			: static_cast<u32>(res_lib.resources.size());

		const auto default_state =
			ResourceState{ .state = D3D12_RESOURCE_STATE_COMMON };

		if (!found_spot)
			res_lib.resources.push_back(resource),
			res_lib.allocations.push_back(allocation),
			res_lib.resource_states.push_back(default_state);
		else
			res_lib.resources[spot] = resource, res_lib.allocations[spot] = allocation,
			res_lib.resource_states[spot] = default_state;

		return handle;
	}

	// only release staging resources! resources that have views need to flush their view cache which is not implemented yet :)
	auto Context::release_resource(Handle handle)-> void {
		res_lib.allocations[handle] = nullptr;
		res_lib.resources[handle] = nullptr;
	}

	Resource<Buffer> Context::create_buffer(const BufferCreateInfo& create_info, D3D12_RESOURCE_STATES initial_state) {
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
		case MemoryUsage::GPU_Writable:
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
			state = D3D12_RESOURCE_STATE_COMMON;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			break;
		case MemoryUsage::CPU_Readable:
			allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
			state = D3D12_RESOURCE_STATE_GENERIC_READ;
			break;
		}
		state = initial_state == D3D12_RESOURCE_STATE_COMMON ? state : initial_state;

		ComPtr<D3D12MA::Allocation> allocation;
		ComPtr<ID3D12Resource> resource;

		DX_CHECK(allocator->CreateResource(&allocation_desc, &resource_desc, state,
			nullptr, &allocation,
			IID_PPV_ARGS(&resource)));

		const u32 handle = RegisterResource(resource, allocation);

		return Resource<Buffer>(handle);
	}

	[[nodiscard]] auto TextureExtent::full_swap_chain() -> TextureExtent {
		return TextureExtent{
			.width = c.swap_chain.width,
			.height = c.swap_chain.height,
		};
	}


	Resource<D2> Context::create_texture_2d(TextureCreateInfo&& texture_info) {
		D3D12_RESOURCE_FLAGS res_flags = D3D12_RESOURCE_FLAG_NONE;
		if (texture_info.usage == TextureUsage::RENDER_TARGET) {
			res_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
		else if (texture_info.usage == TextureUsage::DEPTH_STENCIL) {
			res_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
		if (texture_info.usage == TextureUsage::SHADER_READ_WRITE_ATOMIC) {
			res_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		// TODO: mip levels hard coded right now :(
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
			texture_info.format, texture_info.extent.width,
			texture_info.extent.height, 1, 1, 1, 0,
			res_flags);
		

		ComPtr<D3D12MA::Allocation> allocation;
		ComPtr<ID3D12Resource> resource;

		// bruh lol
		const D3D12MA::ALLOCATION_DESC allocation_desc = {
			.HeapType = D3D12_HEAP_TYPE_DEFAULT,
		};
		const D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ;

		DX_CHECK(allocator->CreateResource(&allocation_desc, &desc, state, nullptr,
			&allocation, IID_PPV_ARGS(&resource)));

		const u32 handle = RegisterResource(resource, allocation);
		auto res = Resource<D2>(handle);
		auto& s = get_res_state(res);
		s.state = state;
		return res;
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
		auto& heap = c.res_lib.storage.bindable_desc_heap;
		auto& lib = c.res_lib;
		return lib.acceleration_structure_cache.contains(*this)
			? heap.get_index_of(lib.acceleration_structure_cache[*this])
			: heap.push_back(*this);
	};

	[[nodiscard]] auto ResourceViewInfo::desc_index() const -> u32 {
		auto* heap = &c.res_lib.storage.bindable_desc_heap;
		auto& lib = c.res_lib;
		if (type == ResourceType::Buffer) {
			return lib.buffer_view_cache.contains(views.buffer_view)
				? heap->get_index_of(lib.buffer_view_cache[views.buffer_view])
				: heap->push_back(*this);
		}
		const auto usage = views.texture_view.texture_usage;
		if (usage == TextureUsage::RENDER_TARGET) {
			heap = &c.res_lib.storage.render_target_heap;
		}
		else if (usage == TextureUsage::DEPTH_STENCIL) {
			heap = &c.res_lib.storage.depth_stencil_heap;
		}
		return lib.texture_view_cache.contains(views.texture_view)
			? heap->get_index_of(lib.texture_view_cache[views.texture_view])
			: heap->push_back(*this);
	}

	[[nodiscard]] auto ResourceViewInfo::desc_handle() const
		-> D3D12_CPU_DESCRIPTOR_HANDLE {
		auto* heap = &c.res_lib.storage.bindable_desc_heap;
		auto& lib = c.res_lib;
		if (type == ResourceType::Buffer) {
			return lib.buffer_view_cache.contains(views.buffer_view)
				? lib.buffer_view_cache[views.buffer_view]
				: heap->push_back_get_handle(*this);
		}
		const auto usage = views.texture_view.texture_usage;
		if (usage == TextureUsage::RENDER_TARGET) {
			heap = &c.res_lib.storage.render_target_heap;
		}
		else if (usage == TextureUsage::DEPTH_STENCIL) {
			heap = &c.res_lib.storage.depth_stencil_heap;
		}
		return lib.texture_view_cache.contains(views.texture_view)
			? lib.texture_view_cache[views.texture_view]
			: heap->push_back_get_handle(*this);
	}

	auto Resource<AccelStructure>::gpu_addr() const -> D3D12_GPU_VIRTUAL_ADDRESS {
		return get_native_res(*this)->GetGPUVirtualAddress();
	}

	auto Resource<Buffer>::map_and_copy(ByteSpan data, usize offset) const -> void {
		void* mapped;
		auto res = d::get_native_res(static_cast<u32>(handle));
		DX_CHECK(res->Map(0, nullptr, &mapped));
		memcpy(static_cast<char*>(mapped) + offset, data.data(), data.size());
		res->Unmap(0, nullptr);
	}

	[[nodiscard]] auto Resource<Buffer>::gpu_addr() const ->D3D12_GPU_VIRTUAL_ADDRESS {
		return get_native_res(*this)->GetGPUVirtualAddress();
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
