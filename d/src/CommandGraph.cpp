#include "d/CommandGraph.h"
#include <fstream>
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
		default:
			access = D3D12_BARRIER_ACCESS_NO_ACCESS;
			break;
		}
		return std::make_pair(sync, access);
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
				else if (metadata.type == AccessType::eReadWriteAtomic || metadata.type == AccessType::eRenderTarget) {
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

	auto CommandGraph::get_barrier_dependencies(const CommandInfo& c0, const CommandInfo& c1) const -> std::vector<Barrier> {
		std::vector<Barrier> barriers;

		// read write connections
		{
			std::vector<Handle> connected_resources;
			std::ranges::set_intersection(c0.reads, c1.writes, std::back_inserter(connected_resources));
			for (const auto& res : connected_resources) {
				const auto [sync0, access0] = recorder.get_barrier_info(c0, res, false);
				const auto [sync1, access1] = recorder.get_barrier_info(c1, res, true);
				barriers.emplace_back(sync0, sync1, access0, access1, res, false);
			}
		}

		// write read connections
		{
			std::vector<Handle> connected_resources;
			std::ranges::set_intersection(c0.writes, c1.reads, std::back_inserter(connected_resources));
			for (const auto& res : connected_resources) {
				const auto [sync0, access0] = recorder.get_barrier_info(c0, res, true);
				const auto [sync1, access1] = recorder.get_barrier_info(c1, res, false);
				barriers.emplace_back(sync0, sync1, access0, access1, res, false);
			}
		}
		return barriers;
	}

	auto CommandGraph::graphify() -> void {
		const auto& stream = recorder.command_stream;
		// build adjacency list
		command_adj_list.resize(stream.size());
		usize ci = 0;
		for (const auto& c0 : stream) {
			usize cii = ci + 1;
			for (const auto& c1 : stream | std::views::drop(ci + 1)) {
				auto barriers = get_barrier_dependencies(c0, c1);
				command_adj_list[ci].emplace_back(cii, barriers);
				++cii;
			}
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
				// the fact that this is a DAG and in-order command list implies this has to be a source
				stack.push(static_cast<u32>(v));
				u32 exe_step = 0;
				while (!stack.empty()) { // dfs
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

	auto CommandGraph::execute_async() -> std::future<Empty> {
		return std::future<Empty>();
	}
}
