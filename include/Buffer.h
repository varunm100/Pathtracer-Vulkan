#pragma once
#include "VkInclude.h"
#include "Common.h"
#include <vk_mem_alloc/vk_mem_alloc.h>
#include <glm/glm.hpp>

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  VkPipelineVertexInputStateCreateFlags flags = 0;
};

// struct Vertex {
//   glm::vec3 position;
//   glm::vec2 uv;

//   Vertex(float x, float y, float z, float ux, float uy);
//   static void getVertexDesc(VertexInputDescription& desc);
// };

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  u32 tex_id;

  Vertex(glm::vec3 _position, glm::vec3 _normal, glm::vec2 _uv, u32 _tex_id);
  Vertex(float px, float py, float pz,
	 float nx, float ny, float nz,
	 float ux, float uy,
	 u32 _tex_id);
};

struct SceneGeometry {
  glm::mat4 transform;
  glm::mat4 transformIT;
  u32 vert_id;
  u32 mat_id;

  SceneGeometry(glm::vec3 pos, u32 vert_id, u32 mat_id);
  SceneGeometry(glm::vec3 pos, glm::vec3 axis, float angle, u32 vert_id, u32 mat_id);
  SceneGeometry(glm::vec3 pos, glm::vec3 axis, float angle, glm::vec3 scale, u32 vert_id, u32 mat_id);
};

struct Material {
  glm::vec3 albedo{0,0,0};
  float emmisive{0};
  float metallic{0};
  float roughness{0};
  float ior{1};
};

struct AllocatedBuffer {
  VkBuffer buffer { VK_NULL_HANDLE };
  VmaAllocation allocation { VK_NULL_HANDLE };

  VkDeviceAddress get_device_addr();
  void create(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage mem_usage=VMA_MEMORY_USAGE_GPU_ONLY);
  void create(VkCommandBuffer cmd, size_t size, const void* data, VkBufferUsageFlags usage, VmaMemoryUsage mem_usage=VMA_MEMORY_USAGE_GPU_ONLY);
  void destroy();
  void* map();
  void unmap();
  
  VkDescriptorBufferInfo get_desc_info(VkDeviceSize offset=0, VkDeviceSize range=VK_WHOLE_SIZE);
};

namespace vkcmd {
  void toBuffer(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void *data);
}
