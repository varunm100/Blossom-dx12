#pragma once

#include <span>

#include "d/stdafx.h"
#include "d/Logging.h"
#include "d/Types.h"
#include "d/CommandList.h"

namespace d {
  enum class QueueType {
    GENERAL,
    ASYNC_COMPUTE,
    ASYNC_TRANSFER,
  };

  struct Queue {
    ComPtr<ID3D12CommandQueue> handle;
    ComPtr<ID3D12Fence> idle_fence;
    HANDLE idle_event;
    QueueType type{QueueType::GENERAL};
    UINT fence_val{0};

    Queue(QueueType type);
    Queue() = default;
    ~Queue() { CloseHandle(idle_event); }
    auto init(QueueType type) -> void;
    auto submit_lists(std::initializer_list<CommandList> lists) -> void;
    auto block_until_idle(u32 timeout=INFINITE) -> void;
    [[nodiscard]] auto get_command_list() -> d::CommandList;
  };
}