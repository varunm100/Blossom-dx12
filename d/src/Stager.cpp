//
// Created by varun on 5/2/2022.
//

#include <algorithm>
#include <filesystem>
#define NOMINMAX
#include "DirectXTex.h"

#include "d/Stager.h"
#include "d/Context.h"

namespace d {
	Stager::Stager() {
		async_transfer.init(QueueType::GENERAL);
		list = async_transfer.get_command_list();
		max_stage_size = 0u;
	}

	//Resource<D2> Stager::stage_texture_from_file(const char* path) {
	//	using namespace DirectX;
	//	const std::filesystem::path texture_path(path);
	//	TexMetadata metadata;
	//	ScratchImage scratch_image;

	//	if (!std::filesystem::exists(texture_path)) {
	//		err_log("Cannot find texture file: {} ", path);
	//		throw std::exception("File not found.");
	//	}
	//	if (texture_path.extension() == ".dds") {
	//		DX_CHECK(LoadFromDDSFile(texture_path.c_str(), DDS_FLAGS_FORCE_RGB,
	//			&metadata, scratch_image));
	//	}
	//	else if (texture_path.extension() == ".hdr") {
	//		DX_CHECK(LoadFromHDRFile(texture_path.c_str(), &metadata, scratch_image));
	//	}
	//	else if (texture_path.extension() == ".tga") {
	//		DX_CHECK(LoadFromTGAFile(texture_path.c_str(), &metadata, scratch_image));
	//	}
	//	else {
	//		DX_CHECK(LoadFromWICFile(texture_path.c_str(), WIC_FLAGS_FORCE_RGB,
	//			&metadata, scratch_image));
	//	}
	//	Resource<D2> texture = c.create_texture_2d(TextureCreateInfo{
	//			.format = DXGI_FORMAT_R8G8B8A8_UNORM,
	//			.dim = TextureDimension::D2,
	//			.extent = TextureExtent{.width = static_cast<u32>(metadata.width),
	//															.height = static_cast<u32>(metadata.height)},
	//			.usage = TextureUsage::SHADER_READ,
	//		});
	//	texture_entries.emplace_back(TextureStageEntry{
	//		.texture = texture,
	//		.metadata = std::move(metadata),
	//		.scratch = std::move(scratch_image),
	//		});
	//	std::vector<D3D12_SUBRESOURCE_DATA> subresources;

	//	//PrepareUpload(c.device, scratch_image.GetImages(), scratch_image.GetImageCount(), metadata,
	//	//	  subresources);

	//	u32 stage_size = static_cast<u32>(GetRequiredIntermediateSize(get_native_res(texture),
	//		0, static_cast<unsigned int>(subresources.size())));
	//	return texture;
	//}

	auto Stager::stage_buffer(Resource<Buffer> dst, ByteSpan data) -> void {
		max_stage_size = std::max(static_cast<u32>(data.size()), max_stage_size);
		buffer_entries.emplace_back(BufferStageEntry{
				.data = data,
				.buffer = dst,
			});
		//return *this;
	}

	auto Stager::stage_block_until_over() -> void {
		Resource<Buffer> stage = c.resource_registry.create_buffer(BufferCreateInfo{
				.size = max_stage_size,
				.usage = MemoryUsage::Mappable,
			});
		// super dumb, but it works for now lmao
		for (auto const entry : buffer_entries) {
			list.record()
				.copy_buffer_region(stage, entry.buffer, entry.data.size())
				.finish();
			stage.map_and_copy(entry.data);
			async_transfer.submit_lists({ list });
			async_transfer.block_until_idle();
		}
		
		c.release_resource(static_cast<Handle>(stage));
		buffer_entries.clear();
		//return *this;
	}
} // namespace d