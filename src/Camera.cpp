#include "Camera.h"
#include "Common.h"
#include "glm/ext.hpp"

Camera::Camera(glm::vec3 initialPos, glm::vec3 dir)
    : m_Pos{initialPos}, m_Dir{dir} {
  cameraData.view = glm::lookAt(m_Pos, glm::vec3(0.0f, 0.0f, 0.0f), m_Up);
  cameraData.proj = glm::perspective(glm::radians(45.0f), (float)1920 / (float)1080, 0.1f, 2000.0f);
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
  float dx = (float) xpos - prevX;
  float dy = (float) -ypos + prevY;
  prevX = (float) xpos;
  prevY =(float) ypos;
  if(!focused) return;
  m_Yaw += (dx * rotSpeed);
  m_Pitch += (dy * rotSpeed);
  if (m_Pitch > 89.0f)
    m_Pitch = 89.0f;
  if (m_Pitch < -89.0f)
    m_Pitch = -89.0f;
  m_Dir = glm::normalize(glm::vec3(cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)), sin(glm::radians(m_Pitch)), sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch))));
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
  //info_log("{}", glm::to_string(this->m_Pos));
  cameraData.view = glm::lookAt(m_Pos, m_Pos + m_Dir, m_Up);
  cameraData.view_inverse = glm::inverse(cameraData.view);
  memcpy(data, &cameraData, sizeof(CameraData));
  //ubo.unmap();
}
