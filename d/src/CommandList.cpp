#include "d/CommandList.h"
#include "d/Context.h"
#include "d/Logging.h"

namespace d {
	auto CommandList::record() -> CommandList& {
		DX_CHECK(allocator->Reset());
		DX_CHECK(handle->Reset(allocator.Get(), nullptr));
		return *this;
	}

	auto CommandList::finish() -> CommandList& {
		DX_CHECK(handle->Close());
		return *this;
	}

	auto CommandList::copy_buffer_region(Resource<Buffer> src, Resource<Buffer> dst,
		usize size, u32 src_offset, u32 dst_offset)
		-> CommandList& {
		handle->CopyBufferRegion(d::get_native_res(dst), dst_offset,
			d::get_native_res(src), src_offset, size);
		return *this;
	}

	auto CommandList::transition(u32 res_handle, D3D12_RESOURCE_STATES after_state) -> CommandList& {
		auto& state = get_res_state(Resource<Buffer>(res_handle));
		if (state.state != after_state) {
			const auto barrier = get_transition(get_native_res(res_handle), state.state, after_state);
			handle->ResourceBarrier(1, &barrier);
			state.state = after_state;
		}
		return *this;
	}

	auto CommandList::transition(Resource<Buffer> buffer, D3D12_RESOURCE_STATES after_state) -> CommandList& {
		transition(static_cast<u32>(buffer), after_state);
		return *this;
	}

	auto CommandList::transition(Resource<D2> texture, D3D12_RESOURCE_STATES after_state) -> CommandList& {
		transition(static_cast<u32>(texture), after_state);
		return *this;
	}

	auto CommandList::draw_directs(const DrawDirectsInfo& render_info)
		-> CommandList& {
		handle->OMSetRenderTargets(1, &render_info.output, false, nullptr);
		handle->SetPipelineState(render_info.pl.get_native());
		handle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		handle->SetDescriptorHeaps(
			1, c.res_lib.storage.bindable_desc_heap.heap.GetAddressOf());
		handle->SetGraphicsRootSignature(render_info.pl.root_signature.Get());
		// constexpr float clear_color[] = { 1.0f, .0f, .0f, 1.0f };
		// c.main_command_list->ClearRenderTargetView(out_handle, clear_color, 0,
		// nullptr);

		for (const auto& draw_info : render_info.draw_infos) {
			handle->SetGraphicsRoot32BitConstants(
				0, static_cast<UINT>(draw_info.push_constants.size() / 4),
				draw_info.push_constants.data(), 0);
			if (draw_info.ibo.has_value()) {
				D3D12_INDEX_BUFFER_VIEW view = draw_info.ibo.value().ibo_view(
					draw_info.start_index.value_or(0u), draw_info.index_count);
				handle->IASetIndexBuffer(&view);
				handle->DrawIndexedInstanced(draw_info.index_count, 1, 0,
					draw_info.start_vertex.value_or(0u), 0);
			}
			else {
				handle->DrawInstanced(draw_info.index_count, 1,
					draw_info.start_vertex.value_or(0u), 0);
			}
		}
		return *this;
	}
} // namespace d
