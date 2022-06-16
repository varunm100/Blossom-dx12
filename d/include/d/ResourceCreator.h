#pragma once

#include "d/Resource.h"

namespace d {

	enum struct MemoryUsage {
		GPU_Writable,
		GPU,
		CPU_Readable,
		Mappable,
	};

	struct BufferCreateInfo {
		size_t size{ 0 };
		MemoryUsage usage{ MemoryUsage::GPU };
	};

	enum struct TextureDimension {
		D1,
		D2,
		D3,
	};

	struct TextureExtent {
		u32 width{ 0 };
		u32 height{ 0 };
		u32 depth{ 1 };
		u32 array_size{ 1 };

		[[nodiscard]] static auto full_swap_chain()->TextureExtent;
	};

	struct TextureCreateInfo {
		DXGI_FORMAT format{ DXGI_FORMAT_UNKNOWN };
		TextureDimension dim{ TextureDimension::D2 };
		TextureExtent extent;
		//    MemoryUsage usage{MemoryUsage::GPU};
		TextureUsage usage;
	};

}
