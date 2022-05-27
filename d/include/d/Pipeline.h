#pragma once

#include <optional>

#include "d/stdafx.h"
#include "d/Types.h"

namespace d {
	struct GraphicsPipeline;

	struct GraphicsPipelineStream {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;

		auto default_raster()->GraphicsPipelineStream&;

		auto set_vertex_shader(const char* label)->GraphicsPipelineStream&;

		auto set_fragment_shader(const char* label)->GraphicsPipelineStream&;

		auto build(bool include_root_constants, std::optional<u32> num_constants)->GraphicsPipeline;
	};

	struct GraphicsPipeline {
		ComPtr<ID3D12PipelineState> pso;
		ComPtr<ID3D12RootSignature> root_signature;
		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;

		GraphicsPipeline() = default;
		~GraphicsPipeline() = default;

		auto get_native() const->ID3D12PipelineState*;
	};

	//struct RayTracingPipeline {
	//	ComPtr<ID3D12StateObject> rt_state;

	//	RayTracingPipeline() = default;
	//	~RayTracingPipeline() = default;

	//	auto get_native() const->ID3D12StateObject*;
	//};

	//struct RayTracingPipelineStream {
	//	struct HitGroup {
	//		std::wstring group_name;
	//		std::wstring closest_hit;
	//		std::wstring any_hit;
	//		std::wstring intersection;
	//		D3D12_HIT_GROUP_DESC native_desc;

	//		//HitGroup(std::wstring _group_name, std::wstring _closest_hit,
	//		//	std::wstring _any_hit=L"", std::wstring _intersection = L"") :
	//		//	group_name(std::move(_group_name)), closest_hit(std::move(_closest_hit)), any_hit(std::move(_any_hit)), intersection(
	//		//		std::move(_intersection))
	//		//{
	//		//	native_desc = D3D12_HIT_GROUP_DESC {
	//		//		.HitGroupExport = group_name.c_str(),
	//		//		.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
	//		//		.ClosestHitShaderImport = closest_hit.c_str(),
	//		//	};
	//		//	if(!any_hit.empty()) native_desc.AnyHitShaderImport = any_hit.c_str();
	//		//	if (!intersection.empty()) {
	//		//		native_desc.Type = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
	//		//		native_desc.IntersectionShaderImport = intersection.c_str();
	//		//	}
	//		//}
	//	};

	//	// pso stuff
	//	CD3DX12_STATE_OBJECT_DESC pso_desc{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };
	//	ComPtr<ID3D12RootSignature> global_root_signature;
	//	ComPtr<ID3D12RootSignature> dummy_root_signature;
	//	std::vector<D3D12_STATE_SUBOBJECT> sub_objects;
	//	std::vector<HitGroup> hit_groups;
	//	u32 max_payload_size;
	//	u32 max_recursion_depth;

	//	// DXIL library
	//	std::string library_label;
	//	std::vector<std::wstring> exported_symbols;
	//	D3D12_DXIL_LIBRARY_DESC library_desc;

	//	RayTracingPipelineStream() = default;
	//	~RayTracingPipelineStream() = default;

	//	auto set_library(const char* label, std::initializer_list<std::wstring> _exported_symbols)->RayTracingPipelineStream;
	//	auto add_hit_group(const char* _group_name, const char* _closest_hit, const char* _any_hit, const char* _intersection)->RayTracingPipelineStream;
	//	auto config_shader(usize _max_payload_size, u32 _max_recursion_depth)->RayTracingPipelineStream;

	//	auto default_stream()->RayTracingPipelineStream;

	//	auto build()->GraphicsPipeline;
	//};

};
