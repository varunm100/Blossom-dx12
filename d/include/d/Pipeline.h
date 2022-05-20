#pragma once

#include <optional>

#include "d/stdafx.h"
#include "d/Types.h"

namespace d {
	struct Pipeline;

	struct GraphicsPipelineStream {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;

		auto default_raster()->GraphicsPipelineStream&;

		auto set_vertex_shader(const char* label)->GraphicsPipelineStream&;

		auto set_fragment_shader(const char* label)->GraphicsPipelineStream&;

		auto build(bool include_root_constants, std::optional<u32> num_constants)->Pipeline;
	};

	struct RayTracingPipelineStream {
		D3D12_RAYTRACING_PIPELINE_CONFIG pso;

		auto build()->Pipeline;
	};

	struct Pipeline {
		ComPtr<ID3D12PipelineState> pso;
		ComPtr<ID3D12RootSignature> root_signature;
		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;

		Pipeline() = default;
		~Pipeline() = default;

		auto get_native()->ID3D12PipelineState*;
	};
};