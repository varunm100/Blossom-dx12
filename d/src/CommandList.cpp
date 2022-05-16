#include "d/CommandList.h"
#include "d/Logging.h"
#include "d/Context.h"

namespace d {
  auto CommandList::record() -> CommandList&{
    DX_CHECK(allocator->Reset());
    DX_CHECK(handle->Reset(allocator.Get(), nullptr));
    return *this;
  }

  auto CommandList::finish() -> CommandList& {
    DX_CHECK(handle->Close());
    return *this;
  }

  auto CommandList::copy_buffer_region(Resource<Buffer> src, Resource<Buffer> dst, usize size, u32 src_offset, u32 dst_offset) -> CommandList& {
    handle->CopyBufferRegion(d::get_native_res(dst), dst_offset, d::get_native_res(src), src_offset, size);
    return *this;
  }

  auto CommandList::draw_directs(const DrawDirectsInfo& render_info) -> CommandList&{
    for(const auto& draw_info : render_info.draw_infos) {
      handle->OMSetRenderTargets(1, &render_info.output, false, nullptr);
      handle->SetPipelineState(render_info.pl.get_native());
      handle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      handle->SetDescriptorHeaps(1, c.res_lib.storage.bindable_desc_heap.heap.GetAddressOf());
      handle->SetGraphicsRootSignature(render_info.pl.root_signature.Get());
      //constexpr float clear_color[] = { 1.0f, .0f, .0f, 1.0f };
      //c.main_command_list->ClearRenderTargetView(out_handle, clear_color, 0, nullptr);
      handle->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(draw_info.push_constants.size() / 4),
                                            draw_info.push_constants.data(), draw_info.constant_offset.value_or(0));
      handle->DrawInstanced(draw_info.index_count, 1, 0, 0);
    }
    return *this;
  }
}

