#include <utility>

#include "d/Pipeline.h"
#include "d/Context.h"

#include <codecvt>
#include <string>

namespace d {
	auto GraphicsPipelineStream::default_raster() -> GraphicsPipelineStream& {
		desc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
			.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
			.SampleMask = 0xffffffff,
			.RasterizerState = D3D12_RASTERIZER_DESC{
				.FillMode = D3D12_FILL_MODE_SOLID,
				.CullMode = D3D12_CULL_MODE_BACK,
				.FrontCounterClockwise = true,
				.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
			},
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.NumRenderTargets = 1,
			.SampleDesc = DXGI_SAMPLE_DESC{
				.Count = 1,
			},
		};
		desc.RTVFormats[0] = c.swap_chain.format;
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.SampleMask = UINT_MAX;

		return *this;
	}

	auto GraphicsPipelineStream::set_vertex_shader(const char* label) -> GraphicsPipelineStream& {
		const ShaderEntry& vs = c.asset_lib.get_shader_asset(label);
		desc.VS.pShaderBytecode = vs.code->GetBufferPointer();
		desc.VS.BytecodeLength = vs.code->GetBufferSize();
		return *this;
	}

	auto GraphicsPipelineStream::set_fragment_shader(const char* label) -> GraphicsPipelineStream& {
		const ShaderEntry& fs = c.asset_lib.get_shader_asset(label);
		desc.PS.pShaderBytecode = fs.code->GetBufferPointer();
		desc.PS.BytecodeLength = fs.code->GetBufferSize();
		return *this;
	}

	auto GraphicsPipelineStream::build(bool include_root_constants, std::optional<u32> num_constants) -> GraphicsPipeline {
		GraphicsPipeline pl{};
		auto root_sign_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{};
		const auto param = D3D12_ROOT_PARAMETER1{
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
				.Constants =
						D3D12_ROOT_CONSTANTS{
								.ShaderRegister = 0,
								.RegisterSpace = 0,
								.Num32BitValues = num_constants.value_or(0),
						},
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		};
		const D3D12_ROOT_PARAMETER1 params[1]{ param };
		root_sign_desc.Init_1_1(include_root_constants ? 1 : 0, include_root_constants ? params : nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED |
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);
		DX_CHECK(D3DX12SerializeVersionedRootSignature(&root_sign_desc,
			D3D_ROOT_SIGNATURE_VERSION_1_1, &pl.rootSignatureBlob,
			&pl.errorBlob));
		DX_CHECK(
			c.device->CreateRootSignature(0, pl.rootSignatureBlob->GetBufferPointer(), pl.rootSignatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&pl.root_signature)));
		desc.pRootSignature = pl.root_signature.Get();
		DX_CHECK(d::c.device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pl.pso)));
		return std::move(pl);
	}

	auto GraphicsPipeline::get_native() const -> ID3D12PipelineState* {
		return pso.Get();
	}

	//auto RayTracingPipelineStream::set_library(const char* label, std::initializer_list<std::wstring> _exported_symbols) -> RayTracingPipelineStream {
	//	library_label = label;
	//	exported_symbols = _exported_symbols;

	//	return *this;
	//}

	//auto RayTracingPipelineStream::add_hit_group(const char* _group_name, const char* _closest_hit, const char* _any_hit, const char* _intersection)->RayTracingPipelineStream {
	//	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> str_converter;
	//	hit_groups.emplace_back(HitGroup(str_converter.from_bytes(_group_name), str_converter.from_bytes(_closest_hit),
	//		str_converter.from_bytes(_any_hit), str_converter.from_bytes(_intersection)));
	//	return *this;
	//}

	//auto RayTracingPipelineStream::config_shader(usize _max_payload_size, u32 _max_recursion_depth)->RayTracingPipelineStream {
	//	max_payload_size = _max_payload_size;
	//	max_recursion_depth = _max_recursion_depth;
	//	return *this;
	//}

	//auto RayTracingPipelineStream::build()-> GraphicsPipeline {
	//	u32 sub_object_count =
	//		1 +									// the one DXIL library
	//		hit_groups.size() + // hit group declarations
	//		1 +									// shader configuration
	//		1 +									// shader payload
	//		0 +									// 0 actual root signatures
	//		2 +									// global and dummy root signature
	//		1;									// final pipeline object

	//	sub_objects = std::vector<D3D12_STATE_SUBOBJECT>(sub_object_count);
	//	u32 current_index = 0;

	//	// add DXIL library
	//	std::vector<D3D12_EXPORT_DESC> dxil_library_exports(exported_symbols.size());
	//	{
	//		for (usize i = 0; i < dxil_library_exports.size(); ++i) {
	//			dxil_library_exports[i] = D3D12_EXPORT_DESC{
	//				.Name = exported_symbols[i].c_str(),
	//				.ExportToRename = nullptr,
	//				.Flags = D3D12_EXPORT_FLAG_NONE,
	//			};
	//		}

	//		const auto& library_code = c.asset_lib.get_shader_asset(library_label.c_str()).code;

	//		library_desc = D3D12_DXIL_LIBRARY_DESC{
	//			.DXILLibrary = D3D12_SHADER_BYTECODE{
	//				.pShaderBytecode = library_code->GetBufferPointer(),
	//				.BytecodeLength = library_code->GetBufferSize(),
	//			},
	//			.NumExports = static_cast<UINT>(dxil_library_exports.size()),
	//			.pExports = dxil_library_exports.data(),
	//		};
	//	}

	//	sub_objects[current_index++] = D3D12_STATE_SUBOBJECT{
	//		.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
	//		.pDesc = &library_desc,
	//	};

	//	// add hit groups
	//	for (const auto& hit_group : hit_groups) {
	//		sub_objects[current_index++] = D3D12_STATE_SUBOBJECT{
	//			.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
	//			.pDesc = &hit_group.native_desc,
	//		};
	//	}

	//	// add shader config
	//	auto shader_desc = D3D12_RAYTRACING_SHADER_CONFIG{
	//		.MaxPayloadSizeInBytes = max_payload_size,
	//		.MaxAttributeSizeInBytes = 0u,
	//	};

	//	const auto shader_config_object = D3D12_STATE_SUBOBJECT{
	//	.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
	//	.pDesc = &shader_desc,
	//	};

	//	sub_objects[current_index++] = shader_config_object;

	//	std::vector<LPCWSTR> exported_symbol_pointers = {};
	//	exported_symbols.reserve(exported_symbols.size());
	//	for (const auto& s : exported_symbols) {
	//		exported_symbol_pointers.push_back(s.c_str());
	//	}
	//	const WCHAR** shader_exports = exported_symbol_pointers.data();

	//	// root signature associations - NONE since we aren't going to use that

	//// Add a subobject for the association between shaders and the payload
	//	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
	//	shaderPayloadAssociation.NumExports = static_cast<UINT>(exported_symbols.size());
	//	shaderPayloadAssociation.pExports = shader_exports;

	//	// Associate the set of shaders with the payload defined in the previous subobject
	//	shaderPayloadAssociation.pSubobjectToAssociate = &sub_objects[(current_index - 1)];

	//	// Create and store the payload association object
	//	D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
	//	shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	//	shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;
	//	sub_objects[current_index++] = shaderPayloadAssociationObject;

	//	// NO ASSOCIATIONS DONE HERE

	//	// create empty global root signature
	//	D3D12_STATE_SUBOBJECT globalRootSig;
	//	globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	//	globalRootSig.pDesc = global_root_signature.GetAddressOf();

	//	sub_objects[current_index++] = globalRootSig;

	//	D3D12_STATE_SUBOBJECT dummyLocalRootSig;
	//	dummyLocalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	//	dummyLocalRootSig.pDesc = dummy_root_signature.GetAddressOf();

	//	sub_objects[current_index++] = dummyLocalRootSig;

	//	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
	//	pipelineConfig.MaxTraceRecursionDepth = max_recursion_depth;

	//	D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
	//	pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	//	pipelineConfigObject.pDesc = &pipelineConfig;

	//	sub_objects[current_index++] = pipelineConfigObject;


	//	D3D12_STATE_OBJECT_DESC pipelineDesc = {};
	//	pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	//	pipelineDesc.NumSubobjects = current_index; // static_cast<UINT>(subobjects.size());
	//	pipelineDesc.pSubobjects = sub_objects.data();

	//	ID3D12StateObject* rtStateObject = nullptr;

	//	// Create the state object
	//	DX_CHECK(c.device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&rtStateObject)));
	//}
}
