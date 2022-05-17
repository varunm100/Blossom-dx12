//
// Created by varun on 5/2/2022.
//

#include "d/Stager.h"
#include "d/Context.h"

namespace d {
Stager::Stager(u32 _max_buffer_size) : max_buffer_size(_max_buffer_size) {
  async_transfer.init(QueueType::GENERAL);
  stage = d::c.create_buffer(BufferCreateInfo{
      .size = max_buffer_size,
      .usage = MemoryUsage::Mappable,
  });
  list = async_transfer.get_command_list();
}

void Stager::stage_buffer(Resource<Buffer> dst, ByteSpan data) {
  stage_entries.emplace_back(StageEntry{
      .data = data,
      .buffer = dst,
  });
}

void Stager::stage_block_until_over() {
  for (auto const entry : stage_entries) {
    list.record()
  			.copy_buffer_region(stage, entry.buffer, entry.data.size())
				.finish();
    stage.map_and_copy(entry.data);
    async_transfer.submit_lists({list});
    async_transfer.block_until_idle();
  }
  stage_entries.clear();
}
} // namespace d