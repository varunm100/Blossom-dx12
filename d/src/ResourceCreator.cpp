#include "d/ResourceCreator.h"
#include "d/Context.h"

namespace d {

	auto ResourceRegistry::create_buffer(const BufferCreateInfo& create_info) const -> Resource<Buffer> {
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
		D3D12_BARRIER_ACCESS access_state = D3D12_BARRIER_ACCESS_COMMON;
		switch (create_info.usage) {
		case MemoryUsage::GPU:
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
			break;
		case MemoryUsage::Mappable:
			allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			break;
		case MemoryUsage::GPU_Writable:
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			break;
		case MemoryUsage::CPU_Readable:
			allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
			break;
		}

		ComPtr<D3D12MA::Allocation> allocation;
		ComPtr<ID3D12Resource> resource;

		DX_CHECK(c.allocator->CreateResource(&allocation_desc, &resource_desc, D3D12_RESOURCE_STATE_COMMON,
			nullptr, &allocation,
			IID_PPV_ARGS(&resource)));

		const u32 handle = c.register_resource(resource, allocation, ResourceState { .type = ResourceType::Buffer, .access_state = access_state });

		return Resource<Buffer>(handle);
	}

	auto create_texture_2d(const TextureCreateInfo& texture_info) -> Resource<D2> {
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

		// TODO: add readback functionality
		const D3D12MA::ALLOCATION_DESC allocation_desc = {
			.HeapType = D3D12_HEAP_TYPE_DEFAULT,
		};

		DX_CHECK(c.allocator->CreateResource(&allocation_desc, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
			&allocation, IID_PPV_ARGS(&resource)));

		const u32 handle = c.register_resource(resource, allocation, ResourceState {.type = ResourceType::D2, .access_state = D3D12_BARRIER_ACCESS_COMMON });
		return Resource<D2>(handle);
	}
	
}

