#include "d/Queue.h"
#include "d/Context.h"

namespace d {

  Queue::Queue(QueueType type) {
    auto queue_desc = D3D12_COMMAND_QUEUE_DESC{
      .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
      .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
      .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
      .NodeMask = 0x0,
    };
    if(type == QueueType::GENERAL) queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    else if(type == QueueType::ASYNC_TRANSFER) queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    else if(type == QueueType::ASYNC_COMPUTE) queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

    DX_CHECK(c.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&handle)));
    DX_CHECK(c.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&idle_fence)));

    idle_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    assert(idle_event && "Queue::submit_lists: could not could create event handle");

    fence_val = 0;
  }

  auto Queue::submit_lists(std::initializer_list<CommandList> lists) -> void {
    std::vector<ID3D12CommandList*> _lists(lists.size());
    std::transform(lists.begin(), lists.end(), _lists.begin(), [](const CommandList& l) { return l.handle.Get(); });

    ++fence_val;
    assert(idle_event && "Queue::submit_lists: could not could create event handle");
    handle->ExecuteCommandLists(lists.size(), _lists.data());
    DX_CHECK(handle->Signal(idle_fence.Get(), fence_val));
    DX_CHECK(idle_fence->SetEventOnCompletion(fence_val, idle_event));
  }

  auto Queue::block_until_idle(u32 timeout) -> void {
    WaitForSingleObject(idle_event, timeout);
  }

  auto Queue::get_command_list() -> d::CommandList {
    d::CommandList list;

    DX_CHECK(c.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&list.allocator)));
    DX_CHECK(c.device->CreateCommandList(0x0, D3D12_COMMAND_LIST_TYPE_DIRECT, list.allocator.Get(), nullptr, IID_PPV_ARGS(&list.handle)));

    DX_CHECK(list.handle->Close());
    return list;
  }

  auto Queue::init(QueueType type) -> void {
    auto queue_desc = D3D12_COMMAND_QUEUE_DESC{
      .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
      .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
      .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
      .NodeMask = 0x0,
    };
    if(type == QueueType::GENERAL) queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    else if(type == QueueType::ASYNC_TRANSFER) queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    else if(type == QueueType::ASYNC_COMPUTE) queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

    DX_CHECK(c.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&handle)));
    DX_CHECK(c.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&idle_fence)));

    idle_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    assert(idle_event && "Queue::submit_lists: could not could create event handle");

    fence_val = 0;
  }
}
