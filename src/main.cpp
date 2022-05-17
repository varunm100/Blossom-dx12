#include <exception>

#define GLFW_EXPOSE_NATIVE_WIN32

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "d/Context.h"
#include "d/Logging.h"
#include "d/Pipeline.h"
#include "d/Queue.h"
#include <DirectXTex.h>

#include <glm/glm.hpp>
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
// Optional. define TINYOBJLOADER_USE_MAPBOX_EARCUT gives robust trinagulation. Requires C++11
//#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"
#include "d/Stager.h"

extern "C" {
	__declspec(dllexport) extern const UINT D3D12SDKVersion = 700;
}
extern "C" {
	__declspec(dllexport) extern const char* D3D12SDKPath = "../../agility_sdk/";
}

struct Vert {
	glm::vec3 pos;
	glm::vec3 normal;

	Vert() = default;
	Vert(float px, float py, float pz, float nx, float ny, float nz) : pos(px, py, pz), normal(nx, ny, nz) {};
};

int main() {
	glfwInit();

	GLFWwindow* window;
	// create window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(1280, 720, "b", nullptr, nullptr);
	if (window == nullptr)
		throw std::exception("glfwCreateWindow failed");

	glfwSetKeyCallback(
		window, [](GLFWwindow* w, const int key, int, const int action, int) {
			if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
				glfwSetWindowShouldClose(w, true);
			}
		});
	d::InitContext(window, 3);
	d::Queue async_transfer = d::Queue(d::QueueType::GENERAL);

	// init buffer
	std::vector<Vert> verts;
	//float verts[12] = {
	//    -0.5, -0.5, 0.0,
	//    0.5, -0.5, 0.0,
	//    -0.5, 0.5, 0.0,
	//    0.5, 0.5, 0.0,
	//};
	std::vector<u32> indices;

	std::string inputfile = "assets/models/kitten.obj";
	tinyobj::ObjReaderConfig reader_config;
	reader_config.mtl_search_path = "./"; // Path to material files
	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(inputfile, reader_config)) {
		if (!reader.Error().empty()) {
			std::cerr << "TinyObjReader: " << reader.Error();
		}
		exit(1);
	}

	if (!reader.Warning().empty()) {
		std::cout << "TinyObjReader: " << reader.Warning();
	}

	auto& attrib = reader.GetAttrib();
	auto& shapes = reader.GetShapes();
	verts.resize(attrib.vertices.size() / 3);
	for (size_t s = 0; s < shapes.size(); s++) {
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			size_t fv = 3;
			for (size_t v = 0; v < fv; v++) {
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
				verts[idx.vertex_index] = Vert(vx, vy, vz, nx, ny, nz);
				indices.emplace_back(idx.vertex_index);

				if (idx.texcoord_index >= 0) {
					tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
					tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
				}
			}
			index_offset += fv;
		}
	}
	auto vert_bytes = ByteSpan(verts);
	auto indices_bytes = ByteSpan(indices);

	auto vbo = d::c.create_buffer(
		d::BufferCreateInfo{ .size = vert_bytes.size(), .usage = d::MemoryUsage::GPU });
	auto ibo = d::c.create_buffer(d::BufferCreateInfo{
			.size = indices_bytes.size(), .usage = d::MemoryUsage::GPU });

	d::Stager stager(std::max(vert_bytes.size(), indices_bytes.size()));
	stager.stage_buffer(vbo, vert_bytes);
	stager.stage_buffer(ibo, indices_bytes);
	stager.stage_block_until_over();

	d::c.library.add_shader("shaders/test.hlsl", d::ShaderType::VERTEX,
		"test_vs");
	d::c.library.add_shader("shaders/test.hlsl", d::ShaderType::FRAGMENT,
		"test_fs");

	struct Constants {
		u32 vbo_index;
		float color1[3];
		float color2[3];
	};
	auto draw_consts = Constants{
			.vbo_index = vbo.read_view(true, 0, vert_bytes.size() / sizeof(u32), 0)
											 .desc_index(),
			.color1 = {0., 1., 0.},
			.color2 = {1., 0., 0.},
	};

	d::Pipeline pl = d::PipelineStream()
		.default_raster()
		.set_vertex_shader("test_vs")
		.set_fragment_shader("test_fs")
		.build(true, sizeof(Constants) / sizeof(u32));

	while (!glfwWindowShouldClose(window)) {
		// handle input
		const auto [out_handle, cl] = d::c.BeginRendering();
		auto draw_info0 = d::DirectDrawInfo{
			//.start_index = 0,
			.index_count = static_cast<u32>(indices_bytes.size() / sizeof(u32)),
			//.start_vertex = 0,
			.ibo = ibo,
			.push_constants = ByteSpan(draw_consts),
		};
		cl.draw_directs(d::DrawDirectsInfo{
				.output = out_handle,
				.draw_infos = {draw_info0},
				.pl = pl,
			});
		d::c.EndRendering();

		glfwPollEvents();
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
