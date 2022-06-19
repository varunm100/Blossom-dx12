#pragma once

#include "d/Resource.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

struct CameraData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 view_inverse;
	glm::mat4 proj_inverse;
};

struct Camera {
	float _yaw = 0.0f, _pitch = 0.0f, _prev_x = 0.0, _prev_y = 0.0;
	glm::vec3 _pos, _up = glm::vec3(0.0f, 1.0f, 0.0f), _dir;
	bool _first = true;
	bool focused = true;
	const float rot_speed = 0.03f;
	const float move_speed = 2.5f;
	d::Resource<d::Buffer> buffer;
	CameraData data;

	Camera(glm::vec3 initial_pos, glm::vec3 look_at, float fov, float aspect_ratio);
	Camera(glm::vec3 initial_pos, glm::vec3 look_at, glm::vec3 up, float aspect_ratio, float fov);
	~Camera() = default;

	auto _check_input(GLFWwindow* window, float dt) -> void;
	auto set_glfw_callbacks(GLFWwindow* window) -> void;
	auto mouse_callback(GLFWwindow* window, double x_pos, double y_pos) -> void;
	auto update(GLFWwindow* window, float dt) -> void;
	[[nodiscard]] auto get_data_index() const -> u32;
};
