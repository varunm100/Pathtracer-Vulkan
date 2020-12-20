#include "Camera.h"
#include "Common.h"
#include "glm/ext.hpp"

Camera::Camera(glm::vec3 initial_pos, glm::vec3 look_at, float fov) {
  m_Pos = initial_pos;
  m_Dir = glm::normalize(look_at - initial_pos);
  m_Pitch = glm::degrees(asin(m_Dir.y));
  m_Yaw = glm::degrees(atan2(m_Dir.z, m_Dir.x));
  
  cameraData.view = glm::lookAt(initial_pos, m_Dir, m_Up);
  cameraData.view_inverse = glm::inverse(cameraData.view);
  cameraData.proj = glm::perspective(glm::radians(fov), (float)1920 / (float)1080, 0.1f, 2000.0f);
  cameraData.proj_inverse = glm::inverse(cameraData.proj);
  cameraData.proj[1][1] *= -1;
  cameraData.proj_inverse[1][1] *= -1;
  ubo.create(sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void Camera::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
  if (first) {
    prevX = (float) xpos;
    prevY = (float) ypos;
    first = false;
  }
  float dx = (float) prevX - xpos;
  float dy = (float) prevY - ypos;
  prevX = (float) xpos;
  prevY =(float) ypos;

  if(!focused) return;

  m_Yaw -= (dx * rotSpeed);
  m_Pitch += (dy * rotSpeed);

  if (m_Pitch > 89.0f)
    m_Pitch = 89.0f;
  if (m_Pitch < -89.0f)
    m_Pitch = -89.0f;

  m_Dir = glm::normalize(glm::vec3(cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)),
				   sin(glm::radians(m_Pitch)),
				   sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch))));
}

void Camera::check_input(GLFWwindow* window, float dt) {
  if(!focused) return;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    m_Pos += m_Dir * moveSpeed * dt;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    m_Pos -= m_Dir * moveSpeed * dt;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    m_Pos += glm::normalize(glm::cross(m_Up, m_Dir)) * moveSpeed * dt;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    m_Pos -= glm::normalize(glm::cross(m_Up, m_Dir)) * moveSpeed * dt;
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    m_Pos += glm::normalize(m_Up) * moveSpeed * dt;
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    m_Pos -= glm::normalize(m_Up) * moveSpeed * dt;
}

void Camera::update_ubo() {
  if(!focused) return;
  glm::mat4 view = glm::lookAt(m_Pos, m_Pos + m_Dir, m_Up);
  if (cameraData.view != view) {
    frame_count = 0;
    cameraData.view = view;
    cameraData.view_inverse = glm::inverse(cameraData.view);
    memcpy(data, &cameraData, sizeof(CameraData));
  }
  //ubo.unmap();
}
