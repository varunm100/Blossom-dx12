#include "d/CommandGraph.h"
#include "d/Context.h"

#include <fstream>
#include <unordered_set>
#include <ranges>
#include <stack>

namespace d {

	[[nodiscard]] inline auto nDrawInfo::get_barrier_info(usize index, bool write) const -> std::pair<D3D12_BARRIER_SYNC, D3D12_BARRIER_ACCESS> {
		const auto& _meta_data = get_meta_data(index, write);
		D3D12_BARRIER_SYNC sync;
		switch (_meta_data.domain) {
		case AccessDomain::eVertex:
			sync = D3D12_BARRIER_SYNC_VERTEX_SHADING;
			break;
		case AccessDomain::eFragment:
			sync = D3D12_BARRIER_SYNC_PIXEL_SHADING;
			break;
		default:
			sync = D3D12_BARRIER_SYNC_NONE;
			break;
		}
		D3D12_BARRIER_ACCESS access;
		switch (_meta_data.type) {
		case AccessType::eRead:
			access = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
			break;
		case AccessType::eReadWriteAtomic:
			access = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
			break;
		case AccessType::eRenderTarget:
			access = D3D12_BARRIER_ACCESS_RENDER_TARGET;
			sync = D3D12_BARRIER_SYNC_RENDER_TARGET;
			break;
		case AccessType::eDepthTarget:
			access = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
			sync = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
			break;
		default:
			access = D3D12_BARRIER_ACCESS_NO_ACCESS;
			break;
		}
		return std::make_pair(sync, access);
	}

	auto nDrawInfo::do_command(CommandList& list) const -> void {
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> render_targets;
		std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> depth_target;
		usize i = 0;
		for (const auto& output : writes) {
			const auto type = get_meta_data(i, true).type;
			if (type == AccessType::eRenderTarget) {
				render_targets.push_back(Resource<D2>(output)
					.rtv_view({})
					.desc_handle());
			}
			else if (type == AccessType::eDepthTarget) {
				depth_target = Resource<D2>(output).dsv_view({}).desc_handle();
			}
			++i;
		}
		list.handle->OMSetRenderTargets(static_cast<UINT>(render_targets.size()), render_targets.data(), false, depth_target.has_value() ? &depth_target.value() : nullptr);

		list.handle->SetPipelineState(pl.get_native());
		list.handle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		list.handle->SetDescriptorHeaps(1, c.resource_registry.storage.bindable_desc_heap.heap.GetAddressOf());
		list.handle->SetGraphicsRootSignature(pl.root_signature.Get());

		i = 0;
		for (const auto& cmd : commands) {
			list.handle->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(push_constants[i].size() / 4), push_constants[i].data(), 0);
			list.handle->IASetIndexBuffer(&cmd.ibo_view);
			list.handle->DrawIndexedInstanced(cmd.index_count_per_instance, cmd.instance_count, cmd.start_index_location, 0, 0);
			++i;
		}
	}

	auto CommandRecorder::draw(const DrawInfo& info) -> CommandRecorder {
		std::vector<Handle> reads;
		std::vector<Handle> writes;
		std::vector<ResourceMetaData> meta_data;
		{
			std::vector<ResourceMetaData> read_meta;
			std::vector<ResourceMetaData> write_meta;
			for (const auto& [handle, metadata] : info.resources) {
				if (metadata.type == AccessType::eRead) reads.emplace_back(handle), read_meta.emplace_back(metadata);
				else if (metadata.type == AccessType::eReadWriteAtomic || metadata.type == AccessType::eRenderTarget || metadata.type == AccessType::eDepthTarget) {
					writes.emplace_back(handle), write_meta.emplace_back(metadata);
				}
			}
			meta_data.insert(meta_data.end(), read_meta.begin(), read_meta.end());
			meta_data.insert(meta_data.end(), write_meta.begin(), write_meta.end());
		}

		u32 index = static_cast<u32>(draw_infos.size());
		draw_infos.emplace_back(nDrawInfo{
			.reads = reads,
			.writes = writes,
			.meta_data = meta_data,
			.push_constants = info.push_constants,
			.commands = info.draw_cmds,
			.debug_name = info.debug_name,
			});
		command_stream.emplace_back(CommandType::eDraw, index, reads, writes);
		return *this;
	}

	auto CommandRecorder::copy_buffer(const CopyBufferInfo& info)->CommandRecorder {
		auto copy_info = nCopyBufferInfo{
			.dst = info.dst,
			.src = info.src,
			.dst_offset = info.dst_offset,
			.src_offset = info.src_offset,
			.num_bytes = info.num_bytes,
		};
		u32 index = static_cast<u32>(copy_buffer_infos.size());
		copy_buffer_infos.emplace_back(copy_info);
		command_stream.emplace_back(CommandType::eDraw, index, std::vector <Handle>{ info.src }, std::vector <Handle>{ info.dst });
		return *this;
	}

	inline auto CommandRecorder::get_draw_info(const CommandInfo& info) const -> nDrawInfo {
		assert_log(info.type == CommandType::eDraw, "Trying to fetch incorrect command type");
		return draw_infos[info.index];
	}

	inline auto CommandRecorder::get_copy_buffer_info(const CommandInfo& info) const -> nCopyBufferInfo {
		assert_log(info.type == CommandType::eCopyBuffer, "Trying to fetch incorrect command type");
		return copy_buffer_infos[info.index];
	}

	auto CommandRecorder::get_barrier_info(const CommandInfo& info, usize index, bool write) const -> std::pair<D3D12_BARRIER_SYNC, D3D12_BARRIER_ACCESS> {
		switch (info.type) {
		case CommandType::eDraw:
			return get_draw_info(info).get_barrier_info(index, write);
		default:
			assert_log(0, "cannot get barrier info from unkown command type");
			return std::make_pair(D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS);
		}
	}

	auto CommandRecorder::get_layout_requirements(const CommandInfo& info) const -> std::vector<std::pair<Handle, D3D12_BARRIER_LAYOUT>> {
		std::vector<std::pair<Handle, D3D12_BARRIER_LAYOUT>> layout_requirements;

		const auto& draw_info = get_draw_info(info);
		usize i = 0u;
		switch (info.type) {
		case CommandType::eDraw:
			for (const auto& read : draw_info.reads) {
				const auto& meta_data = draw_info.get_meta_data(i, false);
				if (get_res_state(read).type == ResourceType::D2) {
					layout_requirements.emplace_back(read, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE);
				}
				++i;
			}
			i = 0;
			for (const auto& write : draw_info.writes) {
				const auto& meta_data = draw_info.get_meta_data(i, true);
				if (get_res_state(write).type == ResourceType::D2) {
					if (meta_data.type == AccessType::eRenderTarget) {
						layout_requirements.emplace_back(write, D3D12_BARRIER_LAYOUT_RENDER_TARGET);
					}
					else if (meta_data.type == AccessType::eDepthTarget) {
						layout_requirements.emplace_back(write, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);
					}
					else if (meta_data.type == AccessType::eReadWriteAtomic) {
						layout_requirements.emplace_back(write, D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS);
					}
				}
				++i;
			}
			return layout_requirements;
		default:
			//assert_log(0, "cannot get barrier info from unkown command type");
			assert(0 && "cannot get barrier info from unknown command type");
			return layout_requirements;
		}

	}

	auto CommandRecorder::do_command(CommandList& list, const CommandInfo& info) const {
		switch (info.type) {
		case CommandType::eDraw:
		{
			const auto& draw_info = get_draw_info(info);
			draw_info.do_command(list);
			break;
		}
		default:
			assert_log(0, "trying to decode unknown command with unknown type");
			return;
		}
	}

	auto CommandGraph::get_barrier_dependencies(const CommandInfo& c0, const CommandInfo& c1) const -> std::vector<Barrier> {
		std::vector<Barrier> barriers;

		// read write connections
		{
			std::vector<Handle> connected_resources;
			std::ranges::set_intersection(c0.reads, c1.writes, std::back_inserter(connected_resources));
			for (const auto& res : connected_resources) {
				const auto [sync0, access0] = recorder.get_barrier_info(c0, res, false);
				const auto [sync1, access1] = recorder.get_barrier_info(c1, res, true);
				const auto res_type = get_res_state(res).type;
				const auto is_texture = !(res_type == ResourceType::Buffer || res_type == ResourceType::AccelStructure);
				barriers.emplace_back(sync0, sync1, access0, access1, res, is_texture);
			}
		}

		// write read connections
		{
			std::vector<Handle> connected_resources;
			std::ranges::set_intersection(c0.writes, c1.reads, std::back_inserter(connected_resources));
			for (const auto& res : connected_resources) {
				const auto [sync0, access0] = recorder.get_barrier_info(c0, res, true);
				const auto [sync1, access1] = recorder.get_barrier_info(c1, res, false);
				const auto is_texture = get_res_state(res).type == ResourceType::D2;
				barriers.emplace_back(sync0, sync1, access0, access1, res, is_texture);
			}
		}
		return barriers;
	}

	auto CommandGraph::graphify() -> void {
		std::unordered_map<Handle, D3D12_BARRIER_LAYOUT> last_layout;
		const auto& stream = recorder.command_stream;

		// build adjacency list
		command_adj_list.resize(stream.size());
		required_layouts.resize(stream.size());

		usize ci = 0;
		for (const auto& c0 : stream) {
			usize cii = ci + 1;
			// fill resource barriers
			for (const auto& c1 : stream | std::views::drop(ci + 1)) {
				auto barriers = get_barrier_dependencies(c0, c1);
				if (!barriers.empty()) command_adj_list[ci].emplace_back(cii, barriers);
				++cii;
			}
			required_layouts[ci] = recorder.get_layout_requirements(c0);
			++ci;
		}
	}

	auto CommandGraph::flatten() -> void {
		const auto& stream = recorder.command_stream;
		// flatten graph into linear stream of commands and barriers
		execution_steps.resize(stream.size());
		std::vector visited(stream.size(), false);

		u32 num_execution_steps = 0;

		std::stack<u32> stack;
		for (usize v = 0; v < visited.size(); ++v) {
			if (!visited[v]) {
				// DAG and In-order command list -> has to be a source node
				stack.push(static_cast<u32>(v));
				u32 exe_step = 0;
				while (!stack.empty()) { // standard dfs
					const u32 node = stack.top();
					stack.pop();
					visited[node] = true;
					auto& step = execution_steps[exe_step];
					step.first.emplace_back(node);
					for (const auto& [next, barrier] : command_adj_list[node]) {
						if (!visited[next])
							stack.push(next);
						step.second.reserve(step.second.size() + barrier.size());
						step.second.insert(step.second.end(), barrier.begin(), barrier.end());
					}

					++exe_step;
				}
				num_execution_steps = std::max(num_execution_steps, exe_step);
			}
		}
		execution_steps.resize(num_execution_steps);

		native_buffer_barriers.resize(num_execution_steps);
		native_texture_barriers.resize(num_execution_steps);
		native_command_level_transitions.resize(stream.size());

		std::unordered_map<Handle, D3D12_BARRIER_LAYOUT> prev_texture_layout;
		for (const auto& step : execution_steps) {
			std::vector<D3D12_BUFFER_BARRIER> buffer_barriers;
			std::vector<D3D12_TEXTURE_BARRIER> texture_barriers;

			std::unordered_map<Handle, D3D12_BARRIER_LAYOUT> first_texture_requirement;
			std::unordered_map<Handle, D3D12_BARRIER_LAYOUT> prev_prev_texture_requirement;
			std::unordered_set<Handle> first_texture_requirement_with_dependency;
			for (const auto& s : step.first)
				for (const auto& requirement : required_layouts[s]) {
					if (!first_texture_requirement.contains(requirement.first))
						first_texture_requirement[requirement.first] = requirement.second, prev_prev_texture_requirement[requirement.first] = get_res_state(requirement.first).layout, prev_texture_layout[requirement.first] =
						requirement.second;

					if (prev_texture_layout[requirement.first] != requirement.second)
						native_command_level_transitions[s].emplace_back(D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_SYNC_NONE,
							D3D12_BARRIER_ACCESS_NO_ACCESS,
							D3D12_BARRIER_ACCESS_NO_ACCESS,
							prev_prev_texture_requirement[requirement.first], requirement.second,
							get_native_res(requirement.first)), prev_texture_layout[
								requirement.first] = requirement.second;
				}

			for (const auto& barrier : step.second) {
				if (barrier.is_texture) {
					texture_barriers.emplace_back(barrier.sync_before, barrier.sync_after, barrier.access_before,
						barrier.access_after, prev_prev_texture_requirement[barrier.res],
						first_texture_requirement.contains(barrier.res)
						? first_texture_requirement[barrier.res]
						: D3D12_BARRIER_LAYOUT_COMMON, get_native_res(barrier.res));
					if (first_texture_requirement.contains(barrier.res)) first_texture_requirement_with_dependency.insert(barrier.res);
				}
				else {
					buffer_barriers.emplace_back(barrier.sync_before, barrier.sync_after, barrier.access_before, barrier.access_after, get_native_res(barrier.res), 0, 0);
				}
			}
			for (const auto& requirement : first_texture_requirement) {
				if (!first_texture_requirement_with_dependency.contains(requirement.first)) {
					texture_barriers.emplace_back(D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_SYNC_NONE,
						D3D12_BARRIER_ACCESS_NO_ACCESS,
						D3D12_BARRIER_ACCESS_NO_ACCESS,
						prev_prev_texture_requirement[requirement.first], requirement.second,
						get_native_res(requirement.first));
				}
			}

			native_buffer_barriers.emplace_back(buffer_barriers);
			native_texture_barriers.emplace_back(texture_barriers);

		}
	}

	auto CommandGraph::do_commands(CommandList& list) const {
		const auto& linear_command_list = recorder.command_stream;

		usize step_i = 0;
		for (const auto& steps : execution_steps | std::views::keys) {
			const auto& barrier_groups = { CD3DX12_BARRIER_GROUP(static_cast<UINT32>(native_buffer_barriers.size()), native_buffer_barriers[step_i].data()),
																																					CD3DX12_BARRIER_GROUP(static_cast<UINT32>(native_texture_barriers.size()), native_texture_barriers[step_i].data()) };
			list.handle->Barrier(static_cast<UINT32>(barrier_groups.size()), data(barrier_groups));
			for (const auto& command_index : steps) {
				const auto& transition_barrier_group = { CD3DX12_BARRIER_GROUP(static_cast<UINT32>(native_command_level_transitions.size()), native_command_level_transitions[command_index].data())};
				list.handle->Barrier(static_cast<UINT32>(transition_barrier_group.size()), data(barrier_groups));
				recorder.do_command(list, linear_command_list[command_index]);
			}
			++step_i;
		}
	}

	auto CommandGraph::visualize_graph_to_image(const char* name) -> void {
		// https://www.graphviz.org/pdf/dotguide.pdf
		std::ofstream out;
		out.open(name, std::ofstream::out | std::ofstream::app);
		out << "digraph G { \n";

		usize v0 = 0;
		for (const auto& links : command_adj_list) {
			auto& c0 = recorder.command_stream[v0];
			std::string_view name0;
			if (c0.type == CommandType::eDraw) name0 = recorder.get_draw_info(c0).debug_name;
			for (const auto& v1 : links | std::views::keys) {
				auto& c1 = recorder.command_stream[v1];
				std::string_view name1;
				if (c1.type == CommandType::eDraw) name1 = recorder.get_draw_info(c1).debug_name;

				out << name0 << " -> " << name1 << "\n";
			}
			++v0;
		}

		out << "}\n";
		//	out << " digraph G { \n \
		 //main -> parse -> execute; \n \
		 //main -> init; \n \
		 //main -> cleanup; \n \
		 //execute -> make_string; \n \
		 //execute -> printf \n \
		 //init -> make_string; \n \
		 //main -> printf; \n \
		 //execute -> compare; \n \
		 //}\n";
		out.close();
	}

	//auto CommandGraph::execute_async() -> std::future<Empty> {
	//	return std::future<Empty>();
	//}
}

