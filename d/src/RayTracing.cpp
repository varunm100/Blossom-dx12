#include <glm/gtc/type_ptr.hpp>

#include "d/RayTracing.h"	
#include "d/Context.h"

#ifndef ROUND_UP
#define ROUND_UP(v, powerOf2Alignment)                                         \
  (((v) + (powerOf2Alignment)-1) & ~((powerOf2Alignment)-1))
#endif

namespace d {

	auto BlasBuilder::add_triangles(const BlasTriangleInfo& create_info) -> BlasBuilder {
		geometries.emplace_back(
			D3D12_RAYTRACING_GEOMETRY_DESC{
				.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
				.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
				.Triangles = D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC {
			.Transform3x4 = create_info.p_transform,
			.IndexFormat = DXGI_FORMAT_R32_UINT,
			.VertexFormat = create_info.vert_format,
			.IndexCount = create_info.index_count,
			.VertexCount = create_info.vertex_count,
			.IndexBuffer = create_info.p_ibo,
			.VertexBuffer =
		D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE {
				.StartAddress = create_info.p_vbo,
				.StrideInBytes = create_info.vbo_stride,
		},
				},
			}
		);
		return *this;
	}

	auto BlasBuilder::add_procedural(const BlasProceduralInfo& create_info) -> BlasBuilder {
		geometries.emplace_back(
			D3D12_RAYTRACING_GEOMETRY_DESC{
				.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS,
				.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
				.AABBs = D3D12_RAYTRACING_GEOMETRY_AABBS_DESC {
				.AABBCount = create_info.num_aabbs,
				.AABBs = create_info.p_aabbs,
				},
			}
		);
		return *this;
	}

	auto BlasBuilder::cmd_build(CommandList& cl, bool _allow_update) -> Resource<AccelStructure> {
		allow_update = _allow_update;
		const auto flags =
			allow_update
			? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
			: D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc;
		prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildDesc.NumDescs = static_cast<UINT>(geometries.size());
		prebuildDesc.pGeometryDescs = geometries.data();
		prebuildDesc.Flags = flags;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};

		c.device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

		// Buffer sizes need to be 256-byte-aligned
		scratch_size =
			ROUND_UP(info.ScratchDataSizeInBytes,
				D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		result_size = ROUND_UP(info.ResultDataMaxSizeInBytes,
			D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		scratch = c.create_buffer(BufferCreateInfo{ .size = scratch_size, .usage = MemoryUsage::GPU_Writable });
		Resource<Buffer> result = c.create_buffer(BufferCreateInfo{ .size = result_size, .usage = MemoryUsage::GPU_Writable }, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);


		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc;
		buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		buildDesc.Inputs.NumDescs = static_cast<UINT>(geometries.size());
		buildDesc.Inputs.pGeometryDescs = geometries.data();
		buildDesc.DestAccelerationStructureData = {
				get_native_res(result)->GetGPUVirtualAddress() };
		buildDesc.ScratchAccelerationStructureData = {
				get_native_res(scratch)->GetGPUVirtualAddress() };
		buildDesc.SourceAccelerationStructureData = 0,
			buildDesc.Inputs.Flags = flags;

		cl.transition(scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cl.handle->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		D3D12_RESOURCE_BARRIER uavBarrier;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = get_native_res(result);
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		cl.handle->ResourceBarrier(1, &uavBarrier);

		return Resource<AccelStructure>(static_cast<u32>(result.handle));

	}

	auto TlasBuilder::add_instance(const TlasInstanceInfo& info) -> TlasBuilder {
		auto instance_desc = D3D12_RAYTRACING_INSTANCE_DESC{
			.InstanceID = info.instance_id,
			.InstanceMask = 0xFF,
			.InstanceContributionToHitGroupIndex = info.hit_index,
			.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE,
			.AccelerationStructure = info.blas.gpu_addr(),
		};
		memcpy(instance_desc.Transform, glm::value_ptr(glm::transpose(info.transform)), sizeof(glm::mat3x4));
		instances.emplace_back(instance_desc);
		return *this;
	}

	auto TlasBuilder::cmd_build(CommandList& list, bool _allow_update) -> Resource<AccelStructure> {
		allow_update = _allow_update;
		const auto flags = allow_update ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
			: D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
			prebuildDesc = {};
		prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildDesc.NumDescs = static_cast<UINT>(instances.size());
		prebuildDesc.Flags = flags;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};

		c.device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

		info.ResultDataMaxSizeInBytes =
			ROUND_UP(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		info.ScratchDataSizeInBytes =
			ROUND_UP(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		result_size = info.ResultDataMaxSizeInBytes;
		scratch_size = info.ScratchDataSizeInBytes;
		desc_size =
			ROUND_UP(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instances.size(),
				D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		instance_buffer = c.create_buffer(BufferCreateInfo{ .size = desc_size, .usage = MemoryUsage::Mappable });
		instance_buffer.map_and_copy(ByteSpan(instances));

		scratch = c.create_buffer(BufferCreateInfo{ .size = scratch_size, .usage = MemoryUsage::GPU_Writable });
		const Resource<Buffer> result = c.create_buffer(BufferCreateInfo{ .size = result_size, .usage = MemoryUsage::GPU_Writable }, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
		buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		buildDesc.Inputs.InstanceDescs = instance_buffer.gpu_addr(),
			buildDesc.Inputs.NumDescs = static_cast<UINT>(instances.size());
		buildDesc.DestAccelerationStructureData = { result.gpu_addr()
		};
		buildDesc.ScratchAccelerationStructureData = { scratch.gpu_addr(),
		};
		buildDesc.SourceAccelerationStructureData = 0;
		buildDesc.Inputs.Flags = flags;

		list.handle->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		D3D12_RESOURCE_BARRIER uavBarrier;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = get_native_res(result);
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		list.handle->ResourceBarrier(1, &uavBarrier);
		return Resource<AccelStructure>(static_cast<u32>(result));
	}
}

