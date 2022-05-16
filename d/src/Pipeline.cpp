#include "d/Pipeline.h"
#include "d/Context.h"

namespace d {
  auto PipelineStream::default_raster() -> PipelineStream & {
    desc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
      .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
      .SampleMask = 0xffffffff,
      .RasterizerState =  D3D12_RASTERIZER_DESC{
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_NONE,
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

  auto PipelineStream::set_vertex_shader(const char *label) -> PipelineStream & {
    ShaderEntry& vs = c.library.get_shader_asset(label);
    desc.VS.pShaderBytecode = vs.code->GetBufferPointer();
    desc.VS.BytecodeLength = vs.code->GetBufferSize();
    return *this;
  }

  auto PipelineStream::set_fragment_shader(const char *label) -> PipelineStream & {
    ShaderEntry& fs = c.library.get_shader_asset(label);
    desc.PS.pShaderBytecode = fs.code->GetBufferPointer();
    desc.PS.BytecodeLength = fs.code->GetBufferSize();
    return *this;
  }

  auto PipelineStream::build(bool include_root_constants, std::optional<u32> num_constants) -> Pipeline {
    Pipeline pl{};
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
    const D3D12_ROOT_PARAMETER1 params[1]{param};
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

  auto Pipeline::get_native() -> ID3D12PipelineState * {
    return pso.Get();
  }
}