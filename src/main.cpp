#include <exception>

#define GLFW_EXPOSE_NATIVE_WIN32

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "d/Logging.h"
#include "d/Context.h"
#include "d/Queue.h"
#include "d/Pipeline.h"
#include <DirectXTex.h>

#include <iostream>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 700; }
extern "C" { __declspec(dllexport) extern const char *D3D12SDKPath = "../../agility_sdk/"; }

int main() {
  glfwInit();

  GLFWwindow *window;
  // create window
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(1280, 720, "b", nullptr, nullptr);
  if (window == nullptr) throw std::exception("glfwCreateWindow failed");

  glfwSetKeyCallback(window, [](GLFWwindow *w, const int key, int, const int action, int) {
    if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(w, true);
    }
  });
  d::InitContext(window, 3);
  d::Queue async_transfer = d::Queue(d::QueueType::GENERAL);

  // init buffer
  float verts[18] = {
    -0.5, -0.5, 0.0,
    0.5, -0.5, 0.0,
    0.0, 0.5, 0.0,

    0.5, 0.0, 0.0,
    1.0, 1.0, 0.0,
    0.0, 0.5, 0.0,
  };

  auto staging = d::c.create_buffer(
    d::BufferCreateInfo{.size = sizeof(verts), .usage = d::MemoryUsage::Mappable});
  auto vbo = d::c.create_buffer(d::BufferCreateInfo{.size = sizeof(verts), .usage = d::MemoryUsage::GPU});
  // copy over resources
  void *mapped;
  DX_CHECK(d::get_native_res(staging)->Map(0, nullptr, &mapped));
  memcpy(mapped, verts, sizeof(verts));
  d::get_native_res(staging)->Unmap(0, nullptr);

  auto cl = async_transfer.get_command_list();
  cl.record()
    .copy_buffer_region(staging, vbo, sizeof(verts))
    .finish();

  async_transfer.submit_lists({cl});
  async_transfer.block_until_idle();

  d::c.library.add_shader("shaders/test.hlsl", d::ShaderType::VERTEX, "test_vs");
  d::c.library.add_shader("shaders/test.hlsl", d::ShaderType::FRAGMENT, "test_fs");


  u32 vbo_index = vbo.read_view(true, 0, 18, 0).desc_index();

  d::Pipeline pl = d::PipelineStream()
                      .default_raster()
                      .set_vertex_shader("test_vs")
                      .set_fragment_shader("test_fs")
                      .build();

  while (!glfwWindowShouldClose(window)) {
    // handle input
    const auto[out_handle, cl] = d::c.BeginRendering();
    cl.draw_directs(d::DrawDirectsInfo{
      .output = out_handle,
      .draw_infos = {d::DirectDrawInfo{.index_count = 6}},
      .pl = pl,
    });
    d::c.EndRendering();

    glfwPollEvents();
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
