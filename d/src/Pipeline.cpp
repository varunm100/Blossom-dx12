#include <utility>

#include "d/Pipeline.h"
#include "d/Context.h"

#include <codecvt>

#include "d/Stager.h"

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

	auto RayTracingPipeline::get_native() const -> ID3D12StateObject* {
		return pso.Get();
	}

	auto RayTracingPipelineStream::set_library(const char* label, std::initializer_list<std::wstring> _exported_symbols) -> RayTracingPipelineStream {
		library_label = label;
		exported_symbols = _exported_symbols;

		return *this;
	}

	auto RayTracingPipelineStream::add_hit_group(std::wstring _group_name, std::wstring _closest_hit, std::wstring _any_hit, std::wstring _intersection)->RayTracingPipelineStream {
		//std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> str_converter;
		hit_groups.emplace_back(_group_name, _closest_hit, _any_hit, _intersection);
		return *this;
	}

	auto RayTracingPipelineStream::add_miss_shader(std::wstring _miss_shader_name)-> RayTracingPipelineStream {
		miss_shaders.emplace_back(_miss_shader_name);
		return *this;
	}

	auto RayTracingPipelineStream::add_callable_shader(std::wstring _callable_shader_name)->RayTracingPipelineStream {
		callable_shaders.emplace_back(_callable_shader_name);
		return *this;
	}

	auto RayTracingPipelineStream::set_ray_gen_shader(std::wstring _ray_gen_shader_name)->RayTracingPipelineStream {
		ray_gen_shader = std::move(_ray_gen_shader_name);
		return *this;
	}

	auto RayTracingPipelineStream::config_shader(usize _max_payload_size, usize _max_attribute_size, u32 _max_recursion_depth)->RayTracingPipelineStream {
		max_payload_size = _max_payload_size;
		max_recursion_depth = _max_recursion_depth;
		max_attribute_size = _max_attribute_size;
		return *this;
	}

	auto RayTracingPipelineStream::build(u32 num_dword_consts)-> RayTracingPipeline {
		u32 sub_object_count =
			1 +									// the one DXIL library
			static_cast<u32>(hit_groups.size()) + // hit group declarations
			1 +									// shader configuration
			1 +									// shader payload
			0 +									// 0 actual root signatures
			2 +									// global and dummy root signature
			1;									// final pipeline object

		sub_objects = std::vector<D3D12_STATE_SUBOBJECT>(sub_object_count);
		u32 current_index = 0;

		// add DXIL library
		std::vector<D3D12_EXPORT_DESC> dxil_library_exports(exported_symbols.size());
		{
			for (usize i = 0; i < dxil_library_exports.size(); ++i) {
				dxil_library_exports[i] = D3D12_EXPORT_DESC{
					.Name = exported_symbols[i].c_str(),
					.ExportToRename = nullptr,
					.Flags = D3D12_EXPORT_FLAG_NONE,
				};
			}

			const auto& library_code = c.asset_lib.get_shader_asset(library_label.c_str()).code;

			library_desc = D3D12_DXIL_LIBRARY_DESC{
				.DXILLibrary = D3D12_SHADER_BYTECODE{
					.pShaderBytecode = library_code->GetBufferPointer(),
					.BytecodeLength = library_code->GetBufferSize(),
				},
				.NumExports = static_cast<UINT>(dxil_library_exports.size()),
				.pExports = dxil_library_exports.data(),
			};
		}

		sub_objects[current_index++] = D3D12_STATE_SUBOBJECT{
			.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
			.pDesc = &library_desc,
		};

		// add hit groups
		for (const auto& hit_group : hit_groups) {
			sub_objects[current_index++] = D3D12_STATE_SUBOBJECT{
				.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
				.pDesc = &hit_group.native_desc,
			};
		}

		// add shader config
		auto shader_desc = D3D12_RAYTRACING_SHADER_CONFIG{
			.MaxPayloadSizeInBytes = static_cast<UINT>(max_payload_size),
			.MaxAttributeSizeInBytes = static_cast<UINT>(max_attribute_size),
		};

		const auto shader_config_object = D3D12_STATE_SUBOBJECT{
		.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
		.pDesc = &shader_desc,
		};

		sub_objects[current_index++] = shader_config_object;

		std::vector<LPCWSTR> exported_symbol_pointers = {};
		exported_symbols.reserve(exported_symbols.size());
		for (const auto& s : exported_symbols) {
			exported_symbol_pointers.push_back(s.c_str());
		}
		const WCHAR** shader_exports = exported_symbol_pointers.data();

		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
		shaderPayloadAssociation.NumExports = static_cast<UINT>(exported_symbols.size());
		shaderPayloadAssociation.pExports = shader_exports;

		shaderPayloadAssociation.pSubobjectToAssociate = &sub_objects[(current_index - 1)];

		D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
		shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;
		sub_objects[current_index++] = shaderPayloadAssociationObject;

		auto rt_pipeline = RayTracingPipeline{};
		{
			ComPtr<ID3DBlob> rootSignatureBlob;
			ComPtr<ID3DBlob> errorBlob;

			auto root_sign_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{};
			const auto param = D3D12_ROOT_PARAMETER1{
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
					.Constants =
							D3D12_ROOT_CONSTANTS{
									.ShaderRegister = 0,
									.RegisterSpace = 0,
									.Num32BitValues = num_dword_consts,
							},
					.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
			};
			const D3D12_ROOT_PARAMETER1 params[1]{ param };
			root_sign_desc.Init_1_1(1, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED |
				D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);
			DX_CHECK(D3DX12SerializeVersionedRootSignature(&root_sign_desc,
				D3D_ROOT_SIGNATURE_VERSION_1_1, &rootSignatureBlob,
				&errorBlob));
			DX_CHECK(
				c.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(),
					IID_PPV_ARGS(&rt_pipeline.global_root_signature)));

			root_sign_desc.Init_1_1(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
			DX_CHECK(D3DX12SerializeVersionedRootSignature(&root_sign_desc,
				D3D_ROOT_SIGNATURE_VERSION_1_1, &rootSignatureBlob,
				&errorBlob));
			DX_CHECK(
				c.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(),
					IID_PPV_ARGS(&rt_pipeline._dummy_root_signature)));

		}

		D3D12_STATE_SUBOBJECT globalRootSig;
		globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
		globalRootSig.pDesc = rt_pipeline.global_root_signature.GetAddressOf();

		sub_objects[current_index++] = globalRootSig;

		D3D12_STATE_SUBOBJECT dummyLocalRootSig;
		dummyLocalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
		dummyLocalRootSig.pDesc = rt_pipeline._dummy_root_signature.GetAddressOf();

		sub_objects[current_index++] = dummyLocalRootSig;

		D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
		pipelineConfig.MaxTraceRecursionDepth = max_recursion_depth;

		D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
		pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		pipelineConfigObject.pDesc = &pipelineConfig;

		sub_objects[current_index++] = pipelineConfigObject;

		D3D12_STATE_OBJECT_DESC pipelineDesc = {};
		pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		pipelineDesc.NumSubobjects = current_index;
		pipelineDesc.pSubobjects = sub_objects.data();

		DX_CHECK(c.device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&rt_pipeline.pso)));

		auto& sbt = rt_pipeline.sbt;
		ComPtr<ID3D12StateObjectProperties> props;
		DX_CHECK(rt_pipeline.pso->QueryInterface(IID_PPV_ARGS(&props)));
		
		assert(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES == 32u);
		constexpr usize ID_SIZE = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		sbt.num_hit_groups = static_cast<u32>(hit_groups.size());
		sbt.num_miss_entries = static_cast<u32>(miss_shaders.size());
		sbt.num_callable_entries = static_cast<u32>(callable_shaders.size());
		//									RAY GEN				|					HIT GROUP TABLE		 				|				MISS SHADER TABLE		        |            CALLABLE SHADER TABLE							|
		usize padding = hit_groups.size() % 2 ? ID_SIZE : 0u;
		usize sbt_size = 2 * ID_SIZE + sbt.num_hit_groups * ID_SIZE + padding + sbt.num_miss_entries * ID_SIZE;

		std::vector<u8> sbt_data(sbt_size);

		std::fill_n(sbt_data.data(), sbt_size, ~0);
		u32 current_byte = 0u;
		memcpy(sbt_data.data() + current_byte, props->GetShaderIdentifier(ray_gen_shader.c_str()), ID_SIZE);
		current_byte += static_cast<u32>(2 * ID_SIZE);
		for(const auto& hit_group : hit_groups) {
			assert(props->GetShaderIdentifier(hit_group.group_name.c_str()));
			memcpy(sbt_data.data() + current_byte, props->GetShaderIdentifier(hit_group.group_name.c_str()), ID_SIZE);
			current_byte += static_cast<u32>(ID_SIZE);
		}
		current_byte += static_cast<u32>(padding);
		for(const auto& miss_shader : miss_shaders) {
			if (!miss_shader.empty())
				assert(props->GetShaderIdentifier(miss_shader.c_str())), memcpy(sbt_data.data() + current_byte,
				                                                                props->GetShaderIdentifier(miss_shader.c_str()),
				                                                                ID_SIZE), current_byte += static_cast<u32>(ID_SIZE);
		}

		// TODO: change this shit

		//sbt.storage = c.create_buffer(BufferCreateInfo{ .size = sbt_size, .usage = MemoryUsage::GPU });

		Stager stager;
		stager.stage_buffer(sbt.storage, ByteSpan(sbt_data));
		stager.stage_block_until_over();
		usize rgen_size = ID_SIZE * 2u;
		usize hit_table_stride = ID_SIZE * 1u;
		usize other_table_stride = ID_SIZE * 1u;
		sbt.p_ray_gen = sbt.storage.gpu_addr_range(rgen_size, 0);
		sbt.p_hit_group_section = sbt.storage.gpu_strided_addr_range(hit_table_stride, hit_table_stride * hit_groups.size(), rgen_size);
		sbt.p_miss_group_section = sbt.storage.gpu_strided_addr_range(other_table_stride,
		                                                              other_table_stride * miss_shaders.size(),
		                                                              rgen_size + hit_table_stride * hit_groups.size() + padding);
		return rt_pipeline;
	}
}
