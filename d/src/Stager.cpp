//
// Created by varun on 5/2/2022.
//

#include "d/Stager.h"

namespace d {
  Stager::Stager() {
    auto desc = D3D12_COMMAND_QUEUE_DESC {
      .Type = D3D12_COMMAND_LIST_TYPE_COPY,
      .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
      .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
      .NodeMask = 0x0,
    };
    c.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&transfer_queue));
  }

  void Stager::open_stage() {

  }

  void Stager::stage_buffer() {

  }

  void Stager::end_stage() {

  }
}