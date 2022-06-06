#pragma once

#include <optional>
#include <variant>

#include "d/Logging.h"
#include "d/Hash.h"
#include "d/Types.h"
#include "d/stdafx.h"

namespace d {
	using Handle = u32;

	enum class Buffer : Handle {};
	enum class D1 : Handle {};
	enum class D2 : Handle {};
	enum class D2MS : Handle {};
	enum class D3 : Handle {};
	enum class D1Array : Handle {};
	enum class D2Array : Handle {};
	enum class D2MSArray : Handle {};
	enum class D3Array : Handle {};
	enum class AccelStructure : Handle {};

	enum class ResourceType : u8 {
		Buffer,
		D1,
		D2,
		D2MS,
		D3,
		D1Array,
		D2Array,
		D2MSArray,
		D3Array,
		AccelStructure,
	};

	template <typename T>
	concept BufferResourceC = std::same_as<T, Buffer>;
	template <typename T>
	concept TextureResourceC = std::same_as<T, D1> || std::same_as<T, D2> ||
		std::same_as<T, D2MS> || std::same_as<T, D3> || std::same_as<T, D1Array> ||
		std::same_as<T, D2Array> || std::same_as<T, D2MSArray> ||
		std::same_as<T, D3Array>;
	template <typename T>
	concept AccelStructureC = std::same_as<T, AccelStructure>;
	template <typename T>
	concept ResourceC =
		BufferResourceC<T> || TextureResourceC<T> || AccelStructureC<T>;

	enum class BufferUsage : u8 {
		SHADER_READ = 0,
		SHADER_READ_WRITE_ATOMIC = 1,
	};

	struct BufferViewInfo {
		Handle resource_handle{ 0 };
		u32 first_element{ 0 };
		u32 num_elements{ 0 };
		u32 stride{ 0 };  // stride == 0 -> raw buffer
		BufferUsage buffer_usage{ BufferUsage::SHADER_READ };
		ResourceType type{ ResourceType::Buffer };

		bool operator==(const BufferViewInfo&) const = default;

		[[nodiscard]] std::variant<D3D12_SHADER_RESOURCE_VIEW_DESC,
			D3D12_UNORDERED_ACCESS_VIEW_DESC>
			get_native_view() const;
	};

	enum class TextureUsage : u8 {
		SHADER_READ,
		SHADER_READ_WRITE_ATOMIC,
		RENDER_TARGET,
		DEPTH_STENCIL,
	};

	struct TextureViewInfo {
		Handle resource_handle{ 0 };
		DXGI_FORMAT format{ DXGI_FORMAT_UNKNOWN };
		u32 first_array_depth_slice{ 0 };
		u32 num_slices{ 1 };
		TextureUsage texture_usage{ TextureUsage::SHADER_READ };
		u32 mip_slice{ 0 };
		u32 num_mips{ 1 };
		u32 highest_quality_mip{ 0 };
		float min_mip_clamp{ 0. };
		ResourceType type;

		bool operator==(const TextureViewInfo&) const = default;

		[[nodiscard]] std::variant<
			D3D12_SHADER_RESOURCE_VIEW_DESC, D3D12_UNORDERED_ACCESS_VIEW_DESC,
			D3D12_RENDER_TARGET_VIEW_DESC, D3D12_DEPTH_STENCIL_VIEW_DESC>
			get_native_view() const;
	};

	template <ResourceC T>
	struct Resource {
		T handle;

		Resource() = default;
		explicit Resource(u32 _handle) : handle(static_cast<T>(_handle)) {}

		operator u32() const { return static_cast<Handle>(handle); }
	};

	struct ResourceViewInfo {
		union {
			BufferViewInfo buffer_view;
			TextureViewInfo texture_view;
		} views;
		ResourceType type{};

		[[nodiscard]] auto desc_index() const->u32;
		[[nodiscard]] auto desc_handle() const->D3D12_CPU_DESCRIPTOR_HANDLE;
	};

	struct AccelerationStructureViewInfo {
		u32 handle;
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV native;

		bool operator==(const AccelerationStructureViewInfo& o) const
		{
			return handle == o.handle && native.Location == o.native.Location;
		}

		[[nodiscard]] D3D12_SHADER_RESOURCE_VIEW_DESC get_native_view() const;

		[[nodiscard]] auto desc_index() const->u32;
	};

	template <>
	struct Resource<AccelStructure> {
		AccelStructure handle;

		Resource() = default;
		explicit Resource(u32 _handle) : handle(static_cast<AccelStructure>(_handle)) {}

		explicit operator u32() const { return static_cast<Handle>(handle); }

		[[nodiscard]] auto gpu_addr() const->D3D12_GPU_VIRTUAL_ADDRESS;
		[[nodiscard]] auto view() const->AccelerationStructureViewInfo
		{
			return AccelerationStructureViewInfo{
				.handle = static_cast<u32>(handle),
				.native = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV {
					.Location = gpu_addr(),
				},
			};
		}
	};

	template <>
	struct Resource<Buffer> {
		Buffer handle;

		Resource() = default;
		explicit Resource(u32 _handle) : handle(static_cast<Buffer>(_handle)) {}

		operator u32() const { return static_cast<Handle>(handle); }

		auto map_and_copy(ByteSpan data, usize offset = 0) const -> void;

		[[nodiscard]] auto gpu_addr() const->D3D12_GPU_VIRTUAL_ADDRESS;

		[[nodiscard]] auto gpu_strided_addr(usize stride) const->D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE;

		[[nodiscard]] auto gpu_addr_range(usize size, usize start_offset=0) const->D3D12_GPU_VIRTUAL_ADDRESS_RANGE;

		[[nodiscard]] auto gpu_strided_addr_range(usize stride, usize size, usize start_offset=0) const->D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE;

		[[nodiscard]] auto ibo_view(std::optional<u32> index_offset,
			u32 num_indices) const
			->D3D12_INDEX_BUFFER_VIEW;

		[[nodiscard]] auto read_view(bool raw, std::optional<u32> first_element,
			u32 num_elements,
			std::optional<u32> stride) const
			-> ResourceViewInfo {
			assert_log(!raw && stride.has_value() || raw, "buffers that are not raw need to have a size");
			return ResourceViewInfo{
					.views = {BufferViewInfo{
							.resource_handle = static_cast<u32>(handle),
							.first_element = first_element.value_or(0),
							.num_elements = num_elements,
							.stride = raw ? 0 : stride.value(),
							.buffer_usage = BufferUsage::SHADER_READ,
							.type = ResourceType::Buffer,
					}},
					.type = ResourceType::Buffer,
			};
		}

		[[nodiscard]] auto read_write_view(bool raw, std::optional<u32> first_element,
			std::optional<u32> num_elements,
			std::optional<u32> stride) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views = {BufferViewInfo{
							.resource_handle = u32(handle),
							.first_element = first_element.value_or(0),
							.num_elements = num_elements.value_or(0),
							.stride = raw ? stride.value() : 0,
							.buffer_usage = BufferUsage::SHADER_READ_WRITE_ATOMIC,
							.type = ResourceType::Buffer,
					}},
					.type = ResourceType::Buffer,
			};
		}
	};

	template <>
	struct Resource<D2> {
		D2 handle;

		explicit Resource(u32 _handle) : handle(static_cast<D2>(_handle)) {}

		operator u32() const { return static_cast<Handle>(handle); }

		[[nodiscard]] auto rtv_view(std::optional<u32> mip_slice) const
			->ResourceViewInfo;

		[[nodiscard]] auto dsv_view(std::optional<u32> mip_slice) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views =
							{
									.texture_view =
											{
													.resource_handle = u32(handle),
													.texture_usage = TextureUsage::DEPTH_STENCIL,
													.mip_slice = mip_slice.value_or(0u),
													.type = ResourceType::D2,
											},
							},
					.type = ResourceType::D2,
			};
		}

		[[nodiscard]] auto read_view(std::optional<u32> highest_quality_mip,
			std::optional<u32> num_mips,
			std::optional<float> min_mip_clamp) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views =
							{
									.texture_view =
											{
													.resource_handle = u32(handle),
													.texture_usage = TextureUsage::SHADER_READ,
													.num_mips = num_mips.value_or(-1),
													.highest_quality_mip = highest_quality_mip.value_or(-1),
													.min_mip_clamp = min_mip_clamp.value_or(0.0f),
													.type = ResourceType::D2,
											},
							},
					.type = ResourceType::D2,
			};
		}

		[[nodiscard]] auto read_write_view(std::optional<u32> mip_slice) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views =
							{
									.texture_view =
											{
													.resource_handle = u32(handle),
													.texture_usage = TextureUsage::SHADER_READ_WRITE_ATOMIC,
													.mip_slice = mip_slice.value_or(0u),
													.type = ResourceType::D2,
											},
							},
					.type = ResourceType::D2,
			};
		}
	};

	template <>
	struct Resource<D2Array> {
		D2Array handle;

		explicit Resource(u32 _handle) : handle(static_cast<D2Array>(_handle)) {}

		[[nodiscard]] auto rtv_view(std::optional<u32> mip_slice,
			std::optional<u32> first_array_slice,
			std::optional<u32> num_slices) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views =
							{
									.texture_view =
											{
													.resource_handle = u32(handle),
													.first_array_depth_slice =
															first_array_slice.value_or(0u),
													.num_slices = num_slices.value_or(1u),
													.texture_usage = TextureUsage::RENDER_TARGET,
													.mip_slice = mip_slice.value_or(0u),
													.type = ResourceType::D2Array,
											},
							},
					.type = ResourceType::D2Array,
			};
		}

		[[nodiscard]] auto dsv_view(std::optional<u32> mip_slice,
			std::optional<u32> first_array_slice,
			std::optional<u32> num_slices) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views =
							{
									.texture_view =
											{
													.resource_handle = u32(handle),
													.first_array_depth_slice =
															first_array_slice.value_or(0u),
													.num_slices = num_slices.value_or(1u),
													.texture_usage = TextureUsage::DEPTH_STENCIL,
													.mip_slice = mip_slice.value_or(0u),
													.type = ResourceType::D2Array,
											},
							},
					.type = ResourceType::D2Array,
			};
		}

		[[nodiscard]] auto read_view(std::optional<u32> highest_quality_mip,
			std::optional<u32> num_mips,
			std::optional<u32> first_array_slice,
			std::optional<u32> num_slices,
			std::optional<float> min_mip_clamp) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views = {.texture_view =
												{
														.resource_handle = u32(handle),
														.first_array_depth_slice =
																first_array_slice.value_or(0),
														.num_slices = num_slices.value_or(1),
														.texture_usage = TextureUsage::SHADER_READ,
														.num_mips = num_mips.value_or(-1),
														.highest_quality_mip =
																highest_quality_mip.value_or(-1),
														.min_mip_clamp = min_mip_clamp.value_or(0.0f),
														.type = ResourceType::D2Array,
												}},
					.type = ResourceType::D2Array,
			};
		}

		[[nodiscard]] auto read_write_view(std::optional<u32> mip_slice,
			std::optional<u32> first_array_slice,
			std::optional<u32> num_slices) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views = {.texture_view =
												{
														.resource_handle = u32(handle),
														.first_array_depth_slice =
																first_array_slice.value_or(0u),
														.num_slices = num_slices.value_or(1u),
														.texture_usage =
																TextureUsage::SHADER_READ_WRITE_ATOMIC,
														.mip_slice = mip_slice.value_or(0u),
														.type = ResourceType::D2Array,
												}},
					.type = ResourceType::D2Array,
			};
		}
	};

	template <>
	struct Resource<D3> {
		D3 handle;

		explicit Resource(u32 _handle) : handle(static_cast<D3>(_handle)) {}

		explicit operator u32() const { return static_cast<Handle>(handle); }

		[[nodiscard]] auto rtv_view(std::optional<u32> mip_slice,
			std::optional<u32> first_depth_slice,
			std::optional<u32> num_slices) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views =
							{
									.texture_view =
											{
													.resource_handle = u32(handle),
													.first_array_depth_slice =
															first_depth_slice.value_or(0u),
													.num_slices = num_slices.value_or(1u),
													.texture_usage = TextureUsage::RENDER_TARGET,
													.mip_slice = mip_slice.value_or(0u),
													.type = ResourceType::D3,
											},
							},
					.type = ResourceType::D3,
			};
		}

		[[nodiscard]] auto read_view(std::optional<u32> highest_quality_mip,
			std::optional<u32> num_mips,
			std::optional<float> min_mip_clamp) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views =
							{
									.texture_view =
											{
													.resource_handle = u32(handle),
													.texture_usage = TextureUsage::SHADER_READ,
													.num_mips = num_mips.value_or(-1),
													.highest_quality_mip = highest_quality_mip.value_or(-1),
													.min_mip_clamp = min_mip_clamp.value_or(0.0f),
													.type = ResourceType::D3,
											},
							},
					.type = ResourceType::D3,
			};
		}

		[[nodiscard]] auto read_write_view(std::optional<u32> mip_slice,
			std::optional<u32> first_depth_slice,
			std::optional<u32> num_slices) const
			-> ResourceViewInfo {
			return ResourceViewInfo{
					.views =
							{
									.texture_view =
											{
													.resource_handle = u32(handle),
													.first_array_depth_slice =
															first_depth_slice.value_or(0u),
													.num_slices = num_slices.value_or(1u),
													.texture_usage = TextureUsage::SHADER_READ_WRITE_ATOMIC,
													.mip_slice = mip_slice.value_or(0u),
													.type = ResourceType::D3,
											},
							},
					.type = ResourceType::D3,
			};
		}
	};

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

}  // namespace d

namespace std {
	template <>
	struct hash<d::BufferViewInfo> {
		std::size_t operator()(const d::BufferViewInfo& x) const noexcept {
			std::size_t h = 0x0;
			hash_combine(h, x.first_element, x.num_elements, x.stride, x.buffer_usage,
				x.resource_handle, x.type);
			return h;
		}
	};

	template <>
	struct hash<d::TextureViewInfo> {
		std::size_t operator()(const d::TextureViewInfo& x) const noexcept {
			std::size_t h = 0x0;
			hash_combine(h, x.format, x.texture_usage, x.mip_slice,
				x.first_array_depth_slice, x.num_slices, x.highest_quality_mip,
				x.num_mips, x.min_mip_clamp, x.resource_handle, x.type);
			return h;
		}
	};

	template <>
	struct hash<d::AccelerationStructureViewInfo> {
		std::size_t operator()(const d::AccelerationStructureViewInfo& x) const noexcept {
			std::size_t h = 0x0;
			hash_combine(h, x.handle, x.native.Location);
			return h;
		}
	};

}  // namespace std