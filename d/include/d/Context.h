#pragma once

#include "d/stdafx.h"

#include <GLFW/glfw3.h>
#include <d/D3D12MemAlloc.h>

#include "d/AssetLibrary.h"
#include "d/Queue.h"
#include "d/Resource.h"

namespace d {
	struct DescHeap {
		ComPtr<ID3D12DescriptorHeap> heap;
		D3D12_CPU_DESCRIPTOR_HANDLE start;
		D3D12_CPU_DESCRIPTOR_HANDLE end;
		usize size{ 0 };
		u32 stride;

		DescHeap() = default;
		auto init(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_desc) -> void;

		auto push_back(const ResourceViewInfo& info)->u32;
		auto push_back_get_handle(const ResourceViewInfo& info)
			->D3D12_CPU_DESCRIPTOR_HANDLE;
		auto get_index_of(D3D12_CPU_DESCRIPTOR_HANDLE handle)->u32;
	};

	struct DescriptorStorage {
		DescHeap render_target_heap;
		DescHeap depth_stencil_heap;
		DescHeap bindable_desc_heap;

		auto init(const u32 num_desc) -> void;
	};

	struct ResourceState {
		D3D12_RESOURCE_STATES state{ D3D12_RESOURCE_STATE_COMMON };
	};

	struct ResourceLibrary {
		std::vector<ResourceState> resource_states;
		std::vector<ComPtr<ID3D12Resource>> resources;
		std::vector<ComPtr<D3D12MA::Allocation>> allocations;

		std::unordered_map<BufferViewInfo, D3D12_CPU_DESCRIPTOR_HANDLE>
			buffer_view_cache;
		std::unordered_map<TextureViewInfo, D3D12_CPU_DESCRIPTOR_HANDLE>
			texture_view_cache;

		DescriptorStorage storage;
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

		Queue async_transfer_q;
		Queue async_compute_q;
		Queue general_q;

		ComPtr<D3D12MA::Allocator> allocator;
		Swapchain swap_chain;
		AssetLibrary library;
		ResourceLibrary res_lib;

		Context() = default;

		~Context() = default;

		auto init(GLFWwindow* window, u32 sc_count) -> void;

		[[nodiscard]] auto
			BeginRendering()->std::pair<Resource<D2>, CommandList&>;

		void EndRendering();

		[[nodiscard]] auto create_buffer(BufferCreateInfo&& create_info)->Resource<Buffer>;

		[[nodiscard]] auto
			create_texture_2d(TextureCreateInfo&& texture_info)->Resource<D2>;

		auto RegisterResource(const ComPtr<ID3D12Resource>& resource,
			const ComPtr<D3D12MA::Allocation>& allocation)->u32;
		auto release_resource(Handle handle) -> void;

		auto
			get_transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before,
				D3D12_RESOURCE_STATES state_after) const->D3D12_RESOURCE_BARRIER;
	};

	extern Context c;

	template <ResourceC T>
	inline auto get_native_res(Resource<T> handle) -> ID3D12Resource* {
		u32 index = static_cast<u32>(handle);
		return c.res_lib.resources[index].Get();
	};
	template <ResourceC T>
	inline auto get_res_state(Resource<T> handle) -> ResourceState& {
		u32 index = static_cast<u32>(handle);
		return c.res_lib.resource_states[index];
	};

	inline auto get_native_res(Handle handle) -> ID3D12Resource* {
		return c.res_lib.resources[handle].Get();
	}

	auto InitContext(GLFWwindow* window, u32 sc_count) -> void;

} // namespace d
