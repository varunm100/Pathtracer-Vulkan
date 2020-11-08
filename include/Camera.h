#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Buffer.h"
#include <GLFW/glfw3.h>

struct CameraData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 view_inverse;
  glm::mat4 proj_inverse;
};

struct Camera
{
  float m_Yaw = 0.0f, m_Pitch = 0.0f, prevX =  1920/2, prevY = 1080/2;
  glm::vec3 m_Pos, m_Up = glm::vec3(0.0f, 1.0f, 0.0f), m_Dir;
  bool first = true;
  const float rotSpeed = 0.015f;
  const float moveSpeed = 0.0025f;
  AllocatedBuffer ubo;
  CameraData cameraData;
  bool focused = true;
  void* data;
  
  Camera(glm::vec3 initialPos, glm::vec3 dir);
  ~Camera() { ubo.unmap(); }

  void mouse_callback(GLFWwindow* window, double xpos, double ypos);
  void check_input(GLFWwindow* window, float dt);
  void update_ubo();
};
