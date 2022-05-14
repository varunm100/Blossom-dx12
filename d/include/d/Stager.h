#pragma once

#include "d/Context.h"
namespace d {
  struct Stager {
    ComPtr<ID3D12CommandQueue> transfer_queue;
    ComPtr<ID3D12CommandList> transfer_list;

    Stager();
    ~Stager() = default;

    void open_stage();
    void stage_buffer();
    void end_stage();
  };

}