#pragma once

#include <glm/glm.hpp>

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
		u64 scratch_size;
		u64 result_size;

		BlasBuilder() = default;
		~BlasBuilder() = default;

		auto add_triangles(const BlasTriangleInfo& create_info) -> BlasBuilder;
		auto cmd_build(CommandList& list, bool _allow_update=false) -> Resource<AccelStructure>;
	};

	struct TlasInstanceInfo {
		const glm::mat3x4& transform;
		u32 instance_id;
		u32 hit_index;
		Resource<AccelStructure> blas;
	};

	struct TlasBuilder {
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;
		Resource<Buffer> scratch;
		Resource<Buffer> instance_buffer;

		bool allow_update;
		u64 scratch_size;
		u64 result_size;
		u64 desc_size;

		TlasBuilder() = default;
		~TlasBuilder() = default;

		auto add_instance(const TlasInstanceInfo& create_info) -> TlasBuilder;
		auto cmd_build(CommandList& list, bool _allow_update=false) -> Resource<AccelStructure>;
	};

}

