#pragma once

#include "d/stdafx.h"

#include <GLFW/glfw3.h>
#include <d/D3D12MemAlloc.h>

#include "d/AssetLibrary.h"
#include "d/Queue.h"
#include "d/Resource.h"
#include "d/ResourceCreator.h"

namespace d {
	struct DescriptorHeap {
		ComPtr<ID3D12DescriptorHeap> heap;
		D3D12_CPU_DESCRIPTOR_HANDLE start;
		D3D12_CPU_DESCRIPTOR_HANDLE end;
		usize size{ 0 };
		u32 stride;

		DescriptorHeap() = default;
		auto init(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_desc) -> void;

		auto push_back(const ResourceViewInfo& info)->u32;
		auto push_back(const AccelerationStructureViewInfo& info) -> u32;
		auto push_back_get_handle(const ResourceViewInfo& info)
			->D3D12_CPU_DESCRIPTOR_HANDLE;
		auto get_index_of(D3D12_CPU_DESCRIPTOR_HANDLE handle)->u32;
	};

	struct DescriptorStorage {
		DescriptorHeap render_target_heap;
		DescriptorHeap depth_stencil_heap;
		DescriptorHeap bindable_desc_heap;

		auto init(u32 num_desc) -> void;
	};

	struct ResourceState {
		ResourceType type;
		D3D12_BARRIER_ACCESS access_state{ D3D12_BARRIER_ACCESS_COMMON };
	};

	struct ResourceRegistry {
		std::vector<ResourceState> resource_states;
		std::vector<ComPtr<ID3D12Resource>> resources;
		std::vector<ComPtr<D3D12MA::Allocation>> allocations;

		std::unordered_map<std::string_view, u32> named_resource_map;

		std::unordered_map<BufferViewInfo, D3D12_CPU_DESCRIPTOR_HANDLE>
			buffer_view_cache;
		std::unordered_map<TextureViewInfo, D3D12_CPU_DESCRIPTOR_HANDLE>
			texture_view_cache;
		std::unordered_map<AccelerationStructureViewInfo, D3D12_CPU_DESCRIPTOR_HANDLE>
			acceleration_structure_cache;

		DescriptorStorage storage;

		auto create_buffer(const BufferCreateInfo& info) const -> Resource<Buffer>;
		auto create_texture_2d(TextureCreateInfo& info) const -> Resource<D2>;
	};

	struct Swapchain {
		ComPtr<IDXGISwapChain1> swapchain;
		std::vector<Resource<D2>> images;
		DXGI_FORMAT format;
		u32 width{ 0 };
		u32 height{ 0 };
		u32 image_index{ 0 };
	};

	struct Context {
		ComPtr<ID3D12Device5> device;
		ComPtr<IDXGIFactory5> factory;

		Queue general_queue;
		// TODO: add multiple queue support
#ifdef _DEBUG
		ComPtr<ID3D12Debug> debug_interface;
		ComPtr<ID3D12Debug1> debug_interface1;
#endif

		CommandList main_command_list;

		Queue general_q;

		ComPtr<D3D12MA::Allocator> allocator;
		Swapchain swap_chain;
		AssetLibrary asset_lib;
		ResourceRegistry resource_registry;

		Context() = default;
		~Context() = default;

		auto init(GLFWwindow* window, u32 sc_count) -> void;

		[[nodiscard]] auto
			BeginRendering()->std::pair<Resource<D2>, CommandList&>;

		void EndRendering();

		auto register_resource(const ComPtr<ID3D12Resource>& resource,
			const ComPtr<D3D12MA::Allocation>& allocation, ResourceState initial_state)->u32;
		auto release_resource(Handle handle) -> void;

	};

	extern Context c;

	inline auto
		get_transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before,
			D3D12_RESOURCE_STATES state_after) ->D3D12_RESOURCE_BARRIER
	{
		auto rd_barrier = D3D12_RESOURCE_BARRIER{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		};
		rd_barrier.Transition.pResource = resource;
		rd_barrier.Transition.StateBefore = state_before;
		rd_barrier.Transition.StateAfter = state_after;
		return rd_barrier;
	}

	template <ResourceC T>
	inline auto get_native_res(Resource<T> handle) -> ID3D12Resource* {
		u32 index = static_cast<u32>(handle);
		return c.resource_registry.resources[index].Get();
	};

	template <ResourceC T>
	[[nodiscard]] inline auto get_named_res(std::string_view name) -> Resource<T> {
		assert_log(c.resource_registry.named_resource_map.contains(name), "rescource with name doesn't exist");
		return Resource<T>(c.resource_registry.named_resource_map[name]);
	};
	template <ResourceC T>
	inline auto get_res_state(Resource<T> handle) -> ResourceState& {
		u32 index = static_cast<u32>(handle);
		return c.resource_registry.resource_states[index];
	};
	[[nodiscard]] inline auto get_res_state(Handle handle) -> ResourceState {
		return c.resource_registry.resource_states[handle];
	};

	inline auto get_native_res(Handle handle) -> ID3D12Resource* {
		return c.resource_registry.resources[handle].Get();
	}

	[[nodiscard]] auto InitContext(GLFWwindow* window, u32 sc_count) -> std::pair<ResourceRegistry&, AssetLibrary&>;

} // namespace d
