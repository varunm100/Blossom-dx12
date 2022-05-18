#pragma once

#include <DirectXTex.h>

#include "d/CommandList.h"
#include "d/Queue.h"
#include "d/Resource.h"
namespace d {
	struct BufferStageEntry {
		ByteSpan data;
		Resource<Buffer> buffer;
	};
	struct TextureStageEntry {
		Resource<D2> texture;
		DirectX::TexMetadata metadata;
		DirectX::ScratchImage scratch;
	};
	struct Stager {
		std::vector<BufferStageEntry> buffer_entries;
		std::vector<TextureStageEntry> texture_entries;
		Queue async_transfer;
		CommandList list;
		u32 max_stage_size;

		Stager();
		~Stager() = default;

		Resource<D2> stage_texture_from_file(const char* path);
		void stage_buffer(Resource<Buffer> dst, ByteSpan data);
		void stage_block_until_over();
	};

}