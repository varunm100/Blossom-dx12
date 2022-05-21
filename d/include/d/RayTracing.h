#pragma once

#include "d/stdafx.h"
#include "d/Types.h"
#include "d/Resource.h"
#include "d/CommandList.h"

namespace d {
	struct BlasTriangleInfo {
		D3D12_GPU_VIRTUAL_ADDRESS p_transform;
		D3D12_GPU_VIRTUAL_ADDRESS p_vbo;
		u32 vertex_count;
		usize vbo_stride;
		D3D12_GPU_VIRTUAL_ADDRESS p_ibo;
		u32 index_count;
		DXGI_FORMAT vert_format{DXGI_FORMAT_R32G32B32_FLOAT};
		bool allow_update{ false };
	};

	struct BlasBuilder {
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries;
		Resource<Buffer> scratch;
		bool allow_update;
		u32 scratch_size;
		u32 result_size;

		BlasBuilder() = default;
		~BlasBuilder() = default;

		auto add_triangles(const BlasTriangleInfo& create_info) -> BlasBuilder;
		auto cmd_build(CommandList& list, bool allow_update) -> Resource<AccelStructure>;
	};

	struct TlasBuilder {
		
	};
}

