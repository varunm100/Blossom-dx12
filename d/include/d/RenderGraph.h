#pragma once

#include "d/Resource.h"
#include "d/CommandList.h"
#include <memory>

namespace d {
  enum class WorkType {
    GRAPHICS,
    COMPUTE,
    RAY_TRACE,
    INPUT,
  };

  struct GraphicsWorkPayload {
  };
  struct ComputeWorkPayload {
  };
  struct RayTracingWorkPayload {
  };
  struct EmptyPayload {
  };
//  template<typename T>
//  concept WorkPayload = std::same_as<T, GraphicsWorkPayload> || std::same_as<T, ComputeWorkPayload> ||
//                        std::same_as<T, RayTracingWorkPayload>;

//  template<WorkPayload T>
//  using WorkPayload = std::v
  struct WorkResourceBundle {
    ComPtr<ID3D12DescriptorHeap> ResourceDescriptorHeap;
    ComPtr<ID3D12DescriptorHeap> SamplerDescriptorHeap;
  };
  struct WorkPayload {
    WorkResourceBundle shader_res;
    ComPtr<ID3D12PipelineState> pso;
    char push_constants[256];
  };

  using BindedResource = std::tuple<Handle, u32>; // offset in descriptor heaps

  struct Work {
    WorkPayload payload;
    WorkType type;
    std::vector<Handle> in_res;
    std::vector<Handle> out_res;

    std::vector<std::shared_ptr<Work>> _in_works;
    std::vector<std::shared_ptr<Work>> _out_works;
  };
  using WorkRef = std::shared_ptr<Work>;

  struct RenderGraph {
    std::vector<WorkRef> nodes;
    std::vector<CommandList> lists;

    RenderGraph() = default;

    ~RenderGraph() = default;

    // work must be added in chronological order
    void add_work(WorkPayload &&_payload, WorkType _type, std::initializer_list<Handle> _in_res,
                  std::initializer_list<Handle> _out_res);

    void add_works(std::initializer_list<Work> works);

    // links will be formed by only looking at subsequent works
    void fill_links();
  };

  struct ExecutableGraph {
    std::vector<WorkRef> nodes;
    // 1 list for each queue (3 queues: general, async transfer, async compute)
  };
}
