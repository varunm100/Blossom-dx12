#pragma once

#include "d/CommandList.h"
#include "d/Queue.h"
#include "d/Resource.h"
namespace d {
  struct StageEntry {
  ByteSpan data;
  Resource<Buffer> buffer;
  };
  struct Stager {
    Resource<Buffer> stage;
    std::vector<StageEntry> stage_entries;
    Queue async_transfer;
    CommandList list;
    u32 max_buffer_size;

    Stager(u32 _max_buffer_size);
    ~Stager() = default;

    void stage_buffer(Resource<Buffer> dst, ByteSpan data);
    void stage_block_until_over();
  };

}