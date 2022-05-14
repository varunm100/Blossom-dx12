#pragma once
#include "d/Queue.h"
#include "d/Resource.h"

namespace d {
  struct ResourceFuture {
    QueueType work_queue; // basically ignore for now since everything's on 1 general queue
    D3D12_RESOURCE_STATES state_before;
    D3D12_BARRIER_SYNC sync_before;
    D3D12_BARRIER_ACCESS access_before;
    D3D12_BARRIER_LAYOUT layout_before;
    Handle res;
  };
}