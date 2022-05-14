#pragma once

#include "d/stdafx.h"

namespace d {
  struct Pipeline;

  struct PipelineStream {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;

    auto default_raster() -> PipelineStream &;

    auto set_vertex_shader(const char *vs) -> PipelineStream &;

    auto set_fragment_shader(const char *fs) -> PipelineStream &;

    auto build() -> Pipeline;
  };

  struct Pipeline {
    ComPtr<ID3D12PipelineState> pso;
    ComPtr<ID3D12RootSignature> root_signature;
    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;

    Pipeline() = default;
    ~Pipeline() = default;

    auto get_native() -> ID3D12PipelineState *;
  };
};