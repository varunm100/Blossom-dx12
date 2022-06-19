#include "d/Context.h"
#include "d/Queue.h"
#include "d/Stager.h"
#include "d/RayTracing.h"
#include "d/CommandGraph.h"
#include "d/ResourceCreator.h"

#include <glm/glm.hpp>

#include <glm/ext/matrix_transform.hpp>

#include "Camera.h"
#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
#include "tiny_obj_loader.h"

struct Vert {
	glm::vec3 pos;
	glm::vec3 normal;

	Vert() = default;
	~Vert() = default;

	Vert(float px, float py, float pz, float nx, float ny, float nz) : pos(px, py, pz), normal(nx, ny, nz) {};
	Vert(float px, float py, float pz) : pos(px, py, pz) {}
};

std::tuple<std::vector<Vert>, std::vector<u32>> load_model(const char* file);

int main() {
	glfwInit();

	GLFWwindow* window;
	// create window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(1280, 720, "b", nullptr, nullptr);
	if (window == nullptr)
		throw std::exception("glfwCreateWindow failed");

	auto [reg, assets] = d::InitContext(window, 3);

	Camera camera(glm::vec3(0., 0., 3), glm::vec3(0.), 45., 1280./720.);
	camera.set_glfw_callbacks(window);

	auto [verts, indices] = load_model("assets/models/kitten.obj");
	auto vert_bytes = ByteSpan(verts);
	auto indices_bytes = ByteSpan(indices);

	// create resources
	d::Resource<d::Buffer> vbo;
	d::Resource<d::Buffer> ibo;
	d::GraphicsPipeline pl;
	{
		using namespace d;

		vbo = reg.create_buffer(BufferCreateInfo{ .size = vert_bytes.size(), .usage = MemoryUsage::GPU });
		ibo = reg.create_buffer(BufferCreateInfo{ .size = indices_bytes.size(), .usage = MemoryUsage::GPU });

		Stager stager;
		stager.stage_buffer(vbo, vert_bytes);
		stager.stage_buffer(ibo, indices_bytes);
		stager.stage_block_until_over();

		assets.add_shader("shaders/test.hlsl", d::ShaderType::VERTEX, "test_vs");
		assets.add_shader("shaders/test.hlsl", d::ShaderType::FRAGMENT, "test_fs");

		pl = GraphicsPipelineStream()
			.default_raster()
			.set_vertex_shader("test_vs")
			.set_fragment_shader("test_fs")
			.build(true, 3);
	}
	d::CommandGraph graph;
	{
		struct DrawConsts {
			u32 vbo_loc;
		};
		using namespace d;
		auto [recorder] = graph.record();
		const auto& output_image = c.swap_chain.images[0];
		recorder.draw(DrawInfo{
			.resources = { vbo.ref(AccessDomain::eVertex), output_image.ref(AccessType::eRenderTarget) },
			.push_constants = {
				ByteSpan(DrawConsts {
					.vbo_loc = vbo.read_view(true, 0, static_cast<u32>(verts.size()), {})
												.desc_index(),
				})
			},
			.draw_cmds = {
				DrawCmd {
					.ibo_view = ibo.ibo_view(0, static_cast<u32>(indices.size())),
					.index_count_per_instance = static_cast<u32>(indices.size()),
					.instance_count = 1,
					.start_index_location = 0,
				},
			},
			.debug_name = "GBuffer",
		});
		graph.graphify();
		graph.flatten();
	}

	auto prev_time = static_cast<float>(glfwGetTime());
	float dt = 0.;
	while (!glfwWindowShouldClose(window)) {
		// physics
		{
			auto current_time = static_cast<float>(glfwGetTime());
			dt = current_time - prev_time;
			prev_time = current_time;
			camera.update(window, dt);
		}
		// rendering
		auto t0 = glfwGetTime() * 1e3;
		{
			const auto [output_image, cl] = d::c.BeginRendering();
			graph.do_commands(cl);
			d::c.EndRendering();
		}
		auto t1 = glfwGetTime() * 1e3;
		glfwSetWindowTitle(window, std::format("b | Render Time: {:.2f} ms", t1 - t0).c_str());
		glfwPollEvents();
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

[[nodiscard]] auto load_model(const char* file) -> std::tuple<std::vector<Vert>, std::vector<u32>> {
	std::string inputfile = file;
	std::vector<u32> indices;
	std::vector<Vert> verts;
	tinyobj::ObjReaderConfig reader_config;
	reader_config.mtl_search_path = "./"; // Path to material files
	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(inputfile, reader_config)) {
		if (!reader.Error().empty()) {
			err_log("TinyObjReader: {}", reader.Error());
			assert(0);
		}
	}

	if (!reader.Warning().empty()) {
		err_log("TinyObjReader: {}", reader.Warning());
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
				//verts[idx.vertex_index] = Vert(vx, vy, vz, nx, ny, nz);
				verts[idx.vertex_index] = Vert(vx, vy, vz);
				indices.emplace_back(idx.vertex_index);

				if (idx.texcoord_index >= 0) {
					tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
					tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
				}
			}
			index_offset += fv;
		}
	}
	return std::make_tuple(verts, indices);
}

extern "C" {
	__declspec(dllexport) extern const UINT D3D12SDKVersion = 700;
}
extern "C" {
	__declspec(dllexport) extern const char* D3D12SDKPath = "../../agility_sdk/";
}
