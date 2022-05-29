#include <glm/ext.hpp>

#include "Camera.h"
#include "d/Context.h"

Camera::Camera(glm::vec3 initial_pos, glm::vec3 look_at, float fov) {
  _pos = initial_pos;
  _dir = normalize(look_at - initial_pos);
  _pitch = glm::degrees(asin(_dir.y));
  _yaw = glm::degrees(atan(_dir.x/ _dir.z));
  
  data.view = lookAt(initial_pos, _dir, _up);
  data.view_inverse = inverse(data.view);
  data.proj = glm::perspective(glm::radians(fov), static_cast<float>(d::c.swap_chain.width) / static_cast<float>(d::c.swap_chain.height), 0.1f, 2000.0f);
  data.proj_inverse = inverse(data.proj);

  buffer = d::c.create_buffer(d::BufferCreateInfo{.size = sizeof(data), .usage = d::MemoryUsage::Mappable});
  buffer.map_and_copy(ByteSpan(data));

  // force caching of descriptor
  u32 t = buffer.read_view(true, 0, static_cast<u32>(sizeof(data)/4), 0).desc_index();
}

Camera::Camera(glm::vec3 initial_pos, glm::vec3 look_at, glm::vec3 up, float aspect_ratio, float fov) {
  _pos = initial_pos;
  _dir = normalize(look_at - initial_pos);
  _pitch = glm::degrees(asin(_dir.y));
  _yaw = glm::degrees(atan(_dir.x/ _dir.z));
  _up = up;
  
  data.view = lookAt(initial_pos, look_at, _up);
  data.view_inverse = inverse(data.view);
  data.proj = glm::perspective(glm::radians(fov), aspect_ratio, 0.1f, 2000.0f);
  data.proj_inverse = inverse(data.proj);

  buffer = d::c.create_buffer(d::BufferCreateInfo{.size = sizeof(data), .usage = d::MemoryUsage::Mappable});
  buffer.map_and_copy(ByteSpan(data));

  // force caching of descriptor
  u32 t = buffer.read_view(true, 0, static_cast<u32>(sizeof(data)/4), 0).desc_index();
}

auto Camera::mouse_callback(GLFWwindow* window, double x_pos, double y_pos) -> void {
  if (_first) {
    _prev_x = static_cast<float>(x_pos);
    _prev_y = static_cast<float>(y_pos);
    _first = false;
  }
  const float dx = static_cast<float>(x_pos) - _prev_x, dy = _prev_y - static_cast<float>(y_pos); // origin is top-left corner
  _prev_x = static_cast<float>(x_pos), _prev_y = static_cast<float>(y_pos);

  if(!focused) return;

  _yaw += (dx * rot_speed);
  _pitch += (dy * rot_speed);

  if (_pitch > 89.0f)
    _pitch = 89.0f;
  if (_pitch < -89.0f)
    _pitch = -89.0f;

  _dir = normalize(glm::vec3(cos(glm::radians(_pitch)) * sin(glm::radians(_yaw)),
				   sin(glm::radians(_pitch)),
				   -cos(glm::radians(_pitch)) * cos(glm::radians(_yaw))));
}

auto Camera::_check_input(GLFWwindow* window, float dt) -> void{
  if(!focused) return;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    _pos += _dir * move_speed * dt;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    _pos -= _dir * move_speed * dt;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    _pos += normalize(glm::cross(_up, _dir)) * move_speed * dt;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    _pos -= normalize(glm::cross(_up, _dir)) * move_speed * dt;
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    _pos += normalize(_up) * move_speed * dt;
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    _pos -= normalize(_up) * move_speed * dt;
}

auto Camera::update(GLFWwindow* window, float dt) -> void{
  if(!focused) return;
  _check_input(window, dt);
  data.view = lookAt(_pos, _pos + _dir, _up);
	data.view_inverse = inverse(data.view);

  buffer.map_and_copy(ByteSpan(data));
}

[[nodiscard]] auto Camera::get_data_index() const -> u32 {
  return buffer.read_view(true, 0, static_cast<u32>(sizeof(data)/4), 0).desc_index();
}


