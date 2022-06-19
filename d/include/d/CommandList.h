#pragma once

#include "d/Pipeline.h"
#include "d/Resource.h"
#include "d/ResourceCreator.h"
#include "d/Types.h"
#include "d/stdafx.h"

#include <glm/glm.hpp>


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
		GraphicsPipeline& pl;
	};

	struct TraceInfo {
		ByteSpan push_constants;
		Resource<D2> output;
		TextureExtent extent;
		RayTracingPipeline& pl;
	};

	struct CommandList {
		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList7> handle;

		CommandList() = default;
		~CommandList() = default;

		auto record()->CommandList&;
		auto finish()->CommandList&;

		auto copy_buffer_region(Resource<Buffer> src, Resource<Buffer> dst,
			usize size, u32 src_offset = 0, u32 dst_offset = 0)
			->CommandList&;

		//auto clear_image(Resource<D2> image, float color[4])->CommandList&;
		//auto copy_image(Resource<D2> src, Resource<D2> dst)->CommandList&;

		//auto transition(u32 res_handle, D3D12_RESOURCE_STATES after_state)->CommandList&;
		//auto transition(Resource<Buffer> buffer, D3D12_RESOURCE_STATES after_state)->CommandList&;
		//auto transition(Resource<D2> texture, D3D12_RESOURCE_STATES after_state)->CommandList&;

		//auto draw_directs(const DrawDirectsInfo& draw_infos)->CommandList&;
		//auto trace(const TraceInfo& trace_info) ->CommandList&;
	};

} // namespace d