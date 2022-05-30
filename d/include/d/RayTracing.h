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

	struct AABB {
		glm::vec3 min;
		glm::vec3 max;

		AABB(float x0, float y0, float z0, float x1, float y1, float z1) : min(glm::vec3(x0, y0, z0)), max(glm::vec3(x1, y1, z1)) {}
		AABB(glm::vec3 _min, glm::vec3 _max) : min(_min), max(_max) {}
	};
	struct BlasProceduralInfo {
		D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE p_aabbs;
		u32 num_aabbs;
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
		auto add_procedural(const BlasProceduralInfo& create_info) -> BlasBuilder;
		auto cmd_build(CommandList& list, bool _allow_update=false) -> Resource<AccelStructure>;
	};

	struct TlasInstanceInfo {
		const glm::mat4& transform;
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

