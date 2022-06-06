#include "d/CommandGraph.h"
#include <fstream>
#include <ranges>
#include <stack>

namespace d {

	//template <usize num_render_targets>
	//auto CommandRecorder::draw(const DrawInfo<num_render_targets>& info)->std::array<Resource<D2>, num_render_targets> {
	//	auto draw_info = _DrawInfo{
	//		.render_targets = info.render_targets,
	//		.commands = info.draw_cmds,
	//		.debug_name = info.debug_name,
	//		.push_constants = info.push_constants,
	//		.shader_reads = info.resources,
	//		.shader_writes = {},
	//	};
	//	u32 index = draw_infos.size();
	//	draw_infos.emplace_back(draw_info);
	//	command_stream.emplace_back(CommandType::eDraw, index);
	//	return info.render_targets;
	//}

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
		command_stream.emplace_back(CommandType::eDraw, index, std::unordered_set<Handle> { info.src }, std::unordered_set<Handle> { info.dst });
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
	auto CommandGraph::flatten() -> void{
		const auto& stream = recorder.command_stream;

		// build adjacency list
		command_adj_list.resize(stream.size());
		usize ci = 0;
		for (const auto& c0 : stream) {
			usize cii = ci + 1;
			for (const auto& c1 : stream | std::views::drop(ci + 1)) {
				const bool read_then_write = std::ranges::any_of(c0.reads, [&](const auto& res) { return c1.writes.contains(res); });
				const bool write_then_read = std::ranges::any_of(c0.writes, [&](const auto& res) { return c1.reads.contains(res); });
				if (read_then_write || write_then_read) command_adj_list[ci].push_back(static_cast<u32>(cii));
				++cii;
			}
			++ci;
		}

		// flatten graph into linear stream of commands and barriers
		execution_steps.resize(stream.size());
		std::vector visited(stream.size(), false);

		u32 num_execution_steps = 0;

		std::stack<u32> stack;
		for (usize v = 0; v < visited.size(); ++v) {
			if (!visited[v]) { 
				// DAG and in-order command list implies this has to be a source
				stack.push(static_cast<u32>(v));
				u32 exe_step = 0;
				while (!stack.empty()) { // dfs
					const u32 node = stack.top();
					stack.pop();
					visited[node] = true;
					auto& step = execution_steps[exe_step];
					step.first.push_back(node);

					for (u32 next : command_adj_list[node])
						if (!visited[next])
							stack.push(next);
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
		for(const auto& links : command_adj_list) {
			auto& c0 = recorder.command_stream[v0];
			std::string_view name0;
			if (c0.type == CommandType::eDraw) name0 = recorder.get_draw_info(c0).debug_name;
			for (const auto& v1 : links) {
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

