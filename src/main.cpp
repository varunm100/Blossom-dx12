#include "d/Context.h"
#include "d/Queue.h"
#include "d/Stager.h"
#include "d/RayTracing.h"

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

	//Vert(float px, float py, float pz, float nx, float ny, float nz) : pos(px, py, pz), normal(nx, ny, nz) {};
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

	d::InitContext(window, 3);

	Camera camera(glm::vec3(0., 0., 1.5), glm::vec3(0.), 30.);
	// setup camera
	{
		glfwSetInputMode(window, GLFW_CURSOR, camera.focused ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		glfwSetWindowUserPointer(window, &camera);
		glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x_pos, double y_pos) {
			auto* c = static_cast<Camera*>(glfwGetWindowUserPointer(window));
			c->mouse_callback(window, x_pos, y_pos);
			});
		glfwSetKeyCallback(
			window, [](GLFWwindow* window, const int key, int, const int action, int) {
				auto* c = static_cast<Camera*>(glfwGetWindowUserPointer(window));
				if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
					glfwSetInputMode(window, GLFW_CURSOR, c->focused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
					c->focused = !c->focused;
				}
				else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
					glfwSetWindowShouldClose(window, true);
				}
			}
		);
	}

	auto [verts, indices] = load_model("assets/models/kitten.obj");
	glm::mat3x4 transform = {
		{1., 0., 0., 0.},
		{0., 1., 0., 0.},
		{0., 0., 1., 0.},
	};

	auto chunk_aabb = d::AABB(0, 0, 0, 8, 8, 8);

	auto vert_bytes = ByteSpan(verts);
	auto indices_bytes = ByteSpan(indices);
	auto transforms_bytes = ByteSpan(transform);
	auto aabb_bytes = ByteSpan(chunk_aabb);

	using namespace d;
	auto vbo = c.create_buffer(BufferCreateInfo{ .size = vert_bytes.size(), .usage = MemoryUsage::GPU });
	auto ibo = c.create_buffer(BufferCreateInfo{ .size = indices_bytes.size(), .usage = MemoryUsage::GPU });
	auto tbo = c.create_buffer(BufferCreateInfo{ .size = transforms_bytes.size(), .usage = MemoryUsage::GPU });

	auto aabb_bo = c.create_buffer(BufferCreateInfo{ .size = aabb_bytes.size(), .usage = MemoryUsage::GPU });

	Stager stager;
	stager.stage_buffer(vbo, vert_bytes);
	stager.stage_buffer(ibo, indices_bytes);
	stager.stage_buffer(tbo, transforms_bytes);
	stager.stage_buffer(aabb_bo, aabb_bytes);
	stager.stage_block_until_over();

	Queue general;
	general.init(QueueType::GENERAL);
	auto list = general.get_command_list();

	list.record();

	auto kitten_blas = BlasBuilder()
		.add_triangles(BlasTriangleInfo{
		.p_transform = tbo.gpu_addr(),
		.p_vbo = vbo.gpu_addr(),
		.vertex_count = static_cast<u32>(verts.size()),
		.vbo_stride = static_cast<u32>(sizeof(Vert)),
		.p_ibo = ibo.gpu_addr(),
		.index_count = static_cast<u32>(indices.size()),
		.vert_format = DXGI_FORMAT_R32G32B32_FLOAT,
		.allow_update = false,
			})
			.cmd_build(list, false);
	auto chunk_blas = BlasBuilder()
		.add_procedural(BlasProceduralInfo{
		.p_aabbs = aabb_bo.gpu_strided_addr(sizeof(d::AABB)),
			.num_aabbs = 1,
			})
			.cmd_build(list, false);
	auto tlas = TlasBuilder()
		.add_instance(TlasInstanceInfo{
			.transform = glm::translate(glm::mat4(1), glm::vec3(-1, 0., -2.)),
			.instance_id = 0,
			.hit_index = 0,
			.blas = kitten_blas,
			})
		//.add_instance(TlasInstanceInfo{
		//	.transform = transform,
		//	.instance_id = 1,
		//	.hit_index = 1,
		//	.blas = chunk_blas,
		//	})
			.cmd_build(list, false);

	list.finish();
	general.submit_lists({ list });
	general.block_until_idle();


	info_log("Built acceleration structures!");

	auto rt_output = c.create_texture_2d(TextureCreateInfo{
		.format = c.swap_chain.format,
		.dim = TextureDimension::D2,
		.extent = TextureExtent::full_swap_chain(),
		.usage = TextureUsage::SHADER_READ_WRITE_ATOMIC
		});

	struct RTConstants {
		u32 tlas;
		u32 output_image;
		u32 camera_index;
	};
	auto tracing_consts = RTConstants{
		.tlas = tlas.view().desc_index(),
		.output_image = rt_output.read_write_view(0).desc_index(),
		.camera_index = camera.get_data_index(),
	};

	c.asset_lib.add_shader("shaders/RT.hlsl", d::ShaderType::LIBRARY, "test_rt_lib");

	auto rt_pl = RayTracingPipelineStream()
		.set_library("test_rt_lib", { L"RayGen", L"ClosestHit", L"Miss" })
		.add_hit_group(L"main", L"ClosestHit")
		.add_miss_shader(L"Miss")
		.set_ray_gen_shader(L"RayGen")
		.config_shader(16u, 8u, 1)
		.build(sizeof(RTConstants) / 4);

	float prev_time = static_cast<float>(glfwGetTime());
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
			const auto [out_handle, cl] = c.BeginRendering();
			cl.trace(TraceInfo{
				.push_constants = ByteSpan(tracing_consts),
				.output = rt_output,
				.extent = TextureExtent::full_swap_chain(),
				.pl = rt_pl,
				})
				.transition(rt_output, D3D12_RESOURCE_STATE_COPY_SOURCE)
				.transition(out_handle, D3D12_RESOURCE_STATE_COPY_DEST)
				.copy_image(rt_output, out_handle);
			c.EndRendering();
		}
		auto t1 = glfwGetTime() * 1e3;
		glfwSetWindowTitle(window, std::format("frame time: {:.2f} ms", t1 - t0).c_str());
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
