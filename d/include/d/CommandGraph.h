#pragma once

#include <future>
#include <ranges>

#include "d/CommandList.h"
#include "d/Resource.h"

namespace d {
	enum class CommandType {
		eDraw,
		eCopyBuffer,
	};

	struct DrawCmd {
		D3D12_INDEX_BUFFER_VIEW ibo_view;
		u32 index_count_per_instance;
		u32 instance_count;
		u32 start_index_location;
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
		std::vector<Handle> reads; // resources read by this command
		std::vector<Handle> writes;// resources written to by this command

		[[nodiscard]] auto read_contains(Handle handle) const -> bool {
			return std::ranges::any_of(reads, [&](const auto& read) { return read == handle;  });
		}
		[[nodiscard]] auto write_contains(Handle handle) const -> bool {
			return std::ranges::any_of(writes, [&](const auto& write) { return write == handle;  });
		}
	};

	struct nDrawInfo {
		std::vector<Handle> reads; // shader reads
		std::vector<Handle> writes; // shader unordered accesses + render targets
		std::vector<ResourceMetaData> meta_data; // in order of read + writes 

		std::vector<ByteSpan> push_constants;
		std::vector<DrawCmd> commands;

		std::string_view debug_name;
		GraphicsPipeline pl;
		bool depth{ false };

		[[nodiscard]] auto get_meta_data(usize index, bool write) const -> ResourceMetaData { return write ? meta_data[index + reads.size()] : meta_data[index]; }
		[[nodiscard]] inline auto get_barrier_info(usize index, bool write) const -> std::pair<D3D12_BARRIER_SYNC, D3D12_BARRIER_ACCESS>;

		auto do_command(CommandList& list) const -> void;
	};

	struct DrawInfo {
		std::initializer_list<std::pair<Handle, ResourceMetaData>> resources;
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

		auto draw(const DrawInfo& info)->CommandRecorder;
		auto copy_buffer(const CopyBufferInfo& info)->CommandRecorder;

		[[nodiscard]] inline auto get_draw_info(const CommandInfo& info) const->nDrawInfo;
		[[nodiscard]] inline auto get_copy_buffer_info(const CommandInfo& info) const->nCopyBufferInfo;
		[[nodiscard]] auto get_barrier_info(const CommandInfo& info, usize index, bool write) const -> std::pair<D3D12_BARRIER_SYNC, D3D12_BARRIER_ACCESS>;
		[[nodiscard]] auto get_layout_requirements(const CommandInfo& info) const -> std::vector<std::pair<Handle, D3D12_BARRIER_LAYOUT>>;
		auto do_command(CommandList& list, const CommandInfo& info) const;
	};

	struct Empty {};

	struct CommandGraph {
		struct Barrier {
			D3D12_BARRIER_SYNC sync_before;
			D3D12_BARRIER_SYNC sync_after;
			D3D12_BARRIER_ACCESS access_before;
			D3D12_BARRIER_ACCESS access_after;
			Handle res;
			bool is_texture;
		};

		CommandRecorder recorder;
		// adj list of execution DAG and flattened version of that groups commands into execution steps
		std::vector<std::vector<std::pair<u32, std::vector<Barrier>>>> command_adj_list;
		std::vector<std::pair<std::vector<u32>, std::vector<Barrier>>> execution_steps;
		// (index of command, array of textures and their required layouts)
		std::vector<std::vector<std::pair<Handle, D3D12_BARRIER_LAYOUT>>> required_layouts;

		// per execution step
		std::vector<std::vector<D3D12_BUFFER_BARRIER>> native_buffer_barriers;
		std::vector<std::vector<D3D12_TEXTURE_BARRIER>> native_texture_barriers;
		// per command
		std::vector<std::vector<D3D12_TEXTURE_BARRIER>> native_command_level_transitions;

		CommandGraph() = default;
		~CommandGraph() = default;

		auto visualize_graph_to_image(const char* name) -> void;

		auto record() -> std::tuple<CommandRecorder&> { return recorder; }

		[[nodiscard]] auto get_barrier_dependencies(const CommandInfo& c0, const CommandInfo& c1) const -> std::vector<Barrier>;
		auto graphify() -> void;
		auto flatten() -> void;
		auto do_commands(CommandList& list) const;
	};
}
