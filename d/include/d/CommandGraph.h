#pragma once

#include <future>
#include <unordered_set>

#include "d/Resource.h"

namespace d {
	enum class CommandType {
		eDraw,
		eCopyBuffer,
	};

	struct DrawCmd {
		Resource<Buffer> ibo;
		u32 index_count_per_instance;
		u32 instance_count;
		u32 start_index_location;
	};

	enum class AccessType {
		READ,
		READ_WRITE_ATOMIC,
	};

	struct nDrawInfo {
		std::vector<Handle> reads; // shader reads
		std::vector<Handle> writes; // render targets + unordered accesses

		std::vector<ByteSpan> push_constants;
		std::vector<DrawCmd> commands;

		std::string_view debug_name;

		void _run_cmd();
	};

	struct CopyBufferInfo;

	struct nCopyBufferInfo {
		Resource<Buffer> dst;
		Resource<Buffer> src;
		u32 dst_offset;
		u32 src_offset;
		u32 num_bytes;

		void _run_cmd();
	};

	struct CommandInfo {
		CommandType type;
		u32 index;
		std::unordered_set<Handle> reads; // resources read by this command
		std::unordered_set<Handle> writes;// resources written to by this command
	};

	template<usize num_render_targets>
	struct DrawInfo {
		std::array<Resource<D2>, num_render_targets> render_targets;
		std::initializer_list<Handle> resources;
		std::initializer_list<ByteSpan> push_constants;
		std::initializer_list<DrawCmd> draw_cmds;
		std::string_view debug_name;
	};

	struct CopyBufferInfo {
		Resource<Buffer> dst;
		Resource<Buffer> src;
		u32 dst_offset;
		u32 src_offset;
		u32 num_bytes;
	};

	struct CommandRecorder {
		std::vector<nDrawInfo> draw_infos;
		std::vector<nCopyBufferInfo> copy_buffer_infos;

		std::vector<CommandInfo> command_stream;

		template<usize num_render_targets>
		auto draw(const DrawInfo<num_render_targets>& info)->std::array<Resource<D2>, num_render_targets> {
			auto draw_info = nDrawInfo{
				.reads = info.resources,
				.writes = std::vector<Handle>(info.render_targets.size()),
				.push_constants = info.push_constants,
				.commands = info.draw_cmds,
				.debug_name = info.debug_name,
			};
			memcpy(draw_info.writes.data(), info.render_targets.data(), info.render_targets.size() * sizeof(Handle));
			u32 index = draw_infos.size();
			draw_infos.emplace_back(draw_info);
			command_stream.emplace_back(CommandType::eDraw, index, std::unordered_set(info.resources.begin(), info.resources.end()), std::unordered_set(draw_info.writes.begin(), draw_info.writes.end()));
			return info.render_targets;
		}
		auto copy_buffer(const CopyBufferInfo& info)->CommandRecorder;

		[[nodiscard]] inline auto get_draw_info(const CommandInfo& info) const->nDrawInfo;
		[[nodiscard]] inline auto get_copy_buffer_info(const CommandInfo& info) const->nCopyBufferInfo;
	};

	struct Empty {};

	struct CommandGraph {
		struct Barrier {
			D3D12_BARRIER_SYNC sync_before;
			D3D12_BARRIER_SYNC sync_after;
			D3D12_BARRIER_ACCESS access_before;
			D3D12_BARRIER_ACCESS access_after;
			bool is_texture;
		};
		CommandRecorder recorder;
		std::vector<std::vector<u32>> command_adj_list;
		std::vector<std::pair<std::vector<u32>, std::vector<Barrier>>> execution_steps;

		CommandGraph() = default;
		~CommandGraph() = default;

		auto visualize_graph_to_image(const char* name) -> void;

		auto record()->std::tuple<CommandRecorder&> { return recorder; }
		auto flatten()->void;
		auto execute_async()->std::future<Empty>;
	};
}
