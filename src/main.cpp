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

extern "C" {
__declspec(dllexport) extern const UINT D3D12SDKVersion = 700;
}
extern "C" {
__declspec(dllexport) extern const char *D3D12SDKPath = "../../agility_sdk/";
}

struct Vert {
  glm::vec3 pos;

  Vert(float x, float y, float z) : pos(x, y, z){};
};

int main() {
  glfwInit();

  GLFWwindow *window;
  // create window
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(1280, 720, "b", nullptr, nullptr);
  if (window == nullptr)
    throw std::exception("glfwCreateWindow failed");

  glfwSetKeyCallback(
      window, [](GLFWwindow *w, const int key, int, const int action, int) {
        if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
          glfwSetWindowShouldClose(w, true);
        }
      });
  d::InitContext(window, 3);
  d::Queue async_transfer = d::Queue(d::QueueType::GENERAL);

  // init buffer
  //Vert verts[4] = {
  //    Vert(-0.5, -0.5, 0.0),
  //    Vert(0.5, -0.5, 0.0),
  //    Vert(-0.5, 0.5, 0.0),
  //    Vert(0.5, 0.5, 0.0),
  //};
  float verts[12] = {
      -0.5, -0.5, 0.0,
      0.5, -0.5, 0.0,
      -0.5, 0.5, 0.0,
      0.5, 0.5, 0.0,
  };
  u32 indices[6] = {0, 1, 2, 1, 3, 2};

  auto staging_vbo = d::c.create_buffer(d::BufferCreateInfo{
      .size = sizeof(verts), .usage = d::MemoryUsage::Mappable});
  auto staging_ibo = d::c.create_buffer(d::BufferCreateInfo{
      .size = sizeof(indices), .usage = d::MemoryUsage::Mappable});

  auto vbo = d::c.create_buffer(
      d::BufferCreateInfo{.size = sizeof(verts), .usage = d::MemoryUsage::GPU});
  auto ibo = d::c.create_buffer(d::BufferCreateInfo{
      .size = sizeof(indices), .usage = d::MemoryUsage::GPU});
  // copy over resources
  staging_vbo.map_and_copy(ByteSpan(verts));
  staging_ibo.map_and_copy(ByteSpan(indices));

  auto cl = async_transfer.get_command_list();
  cl.record()
      .copy_buffer_region(staging_vbo, vbo, ByteSpan(verts).size())
      .copy_buffer_region(staging_ibo, ibo, ByteSpan(indices).size())
      .finish();

  async_transfer.submit_lists({cl});
  async_transfer.block_until_idle();

  d::c.library.add_shader("shaders/test.hlsl", d::ShaderType::VERTEX,
                          "test_vs");
  d::c.library.add_shader("shaders/test.hlsl", d::ShaderType::FRAGMENT,
                          "test_fs");

  struct Constants {
    u32 vbo_index;
    float color1[3];
    float color2[3];
  };
  auto draw_consts = ByteSpan(Constants{
      .vbo_index = vbo.read_view(true, 0, ByteSpan(vbo).size() / sizeof(u32), 0)
                       .desc_index(),
      .color1 = {0., 1., 0.},
      .color2 = {1., 0., 0.},
  });

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
        .index_count = sizeof(indices) / sizeof(u32),
        //.start_vertex = 0,
        .ibo = ibo,
        .push_constants = draw_consts,
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
