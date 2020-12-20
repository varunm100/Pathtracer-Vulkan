#include "Buffer.h"
#include "Context.h"
#include <glm/ext.hpp>
// Vertex::Vertex(float x, float y, float z, float ux, float uy) {
//   position = glm::vec3(x, y, z);
//   uv = glm::vec2(ux, uy);
// // }
// Vertex::Vertex(glm::vec3 _position, glm::vec3 _normal, glm::vec2 _uv,u32 _tex_id)
//     : position{_position}, normal{_normal}, uv{_uv}, tex_id{_tex_id} {}

// Vertex::Vertex(float px, float py, float pz, float nx, float ny, float nz, float ux, float uy, u32 _tex_id) {
//   position = glm::vec3(px, py, pz);
//   normal = glm::vec3(nx, ny, nz);
//   uv = glm::vec2(ux, uy);
//   tex_id = _tex_id;
// }

// void Vertex::getVertexDesc(VertexInputDescription &desc) {
//   VkVertexInputBindingDescription main_binding = {};
//   main_binding.binding = 0;
//   main_binding.stride = sizeof(Vertex);
//   main_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

//   desc.bindings.push_back(main_binding);

//   VkVertexInputAttributeDescription pos_attrib = {};
//   pos_attrib.binding = 0;
//   pos_attrib.location = 0;
//   pos_attrib.format = VK_FORMAT_R32G32B32_SFLOAT;
//   pos_attrib.offset = offsetof(Vertex, position);

//   VkVertexInputAttributeDescription uv_attrib = {};
//   uv_attrib.binding = 0;
//   uv_attrib.location = 1;
//   uv_attrib.format = VK_FORMAT_R32G32_SFLOAT;
//   uv_attrib.offset = offsetof(Vertex, uv);

//   desc.attributes.push_back(pos_attrib);
//   desc.attributes.push_back(uv_attrib);
// }

SceneGeometry::SceneGeometry(glm::vec3 pos, u32 _vert_id, u32 _mat_id)
  : vert_id{_vert_id}, mat_id{_mat_id} {
  transform = glm::translate(glm::mat4(1), pos);
  transformIT = glm::transpose(glm::inverse(transform));
}

SceneGeometry::SceneGeometry(glm::vec3 pos, glm::vec3 axis, float angle, u32 _vert_id, u32 _mat_id)
  : vert_id{_vert_id}, mat_id{_mat_id} {
  transform = glm::translate(glm::mat4(1), pos);
  transform = glm::rotate(transform, angle, axis);
  transformIT = glm::transpose(glm::inverse(transform));
}

SceneGeometry::SceneGeometry(glm::vec3 pos, glm::vec3 axis, float angle, glm::vec3 scale, u32 _vert_id, u32 _mat_id)
    : vert_id{ _vert_id }, mat_id{ _mat_id } {
    transform = glm::translate(glm::mat4(1), pos);
    transform = glm::rotate(transform, angle, axis);
    transform = glm::scale(transform, scale);

    transformIT = glm::transpose(glm::inverse(transform));
}

void AllocatedBuffer::create(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage mem_usage) {
  VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  buffer_info.size = size;
  buffer_info.usage = usage;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = mem_usage;

  VK_CHECK(vmaCreateBuffer(vkallocator, &buffer_info, &alloc_info, &buffer, &allocation, nullptr));
}

void AllocatedBuffer::create(VkCommandBuffer cmd, size_t size, const void* data, VkBufferUsageFlags usage, VmaMemoryUsage mem_usage) {
  create(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, mem_usage);
  vkcmd::toBuffer(cmd, buffer, {0}, size, data);
}

void AllocatedBuffer::destroy() {
  vmaDestroyBuffer(vkallocator, buffer, allocation);
}

void* AllocatedBuffer::map() {
  void *data;
  VK_CHECK(vmaMapMemory(vkallocator, allocation, &data));
  return data;
}

void AllocatedBuffer::unmap() {
  vmaUnmapMemory(vkallocator, allocation);
}

VkDescriptorBufferInfo* AllocatedBuffer::get_desc_info(VkDeviceSize offset, VkDeviceSize range) {
  desc_info = { };
  desc_info.buffer = buffer;
  desc_info.offset = offset;
  desc_info.range  = range;
  
  return &desc_info;
}

void AllocatedBuffer::fill_desc_infos(AllocatedBuffer* buffers, VkDescriptorBufferInfo* buffer_infos, u32 count) {
  for (u32 i = 0; i < count; ++i) {
    buffer_infos[i] = {};
    buffer_infos[i].buffer = buffers[i].buffer;
    buffer_infos[i].offset = 0;
    buffer_infos[i].range = VK_WHOLE_SIZE;
  }
}

VkDeviceAddress AllocatedBuffer::get_device_addr() {
  VkBufferDeviceAddressInfoKHR addr_info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
  addr_info.buffer = buffer;

  return vkGetBufferDeviceAddressKHR(vkcontext.device, &addr_info);
}

namespace vkcmd {
  void toBuffer(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void *data) {
    if (!size || !buffer)
      return;
  
    AllocatedBuffer staging;
    staging.create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void *mapping = staging.map();
    memcpy(mapping, data, size);
    staging.unmap();
    VkBufferCopy cpy {};
    cpy.size = size;
    cpy.srcOffset = 0;
    cpy.dstOffset = offset;
    vkCmdCopyBuffer(cmd, staging.buffer, buffer, 1, &cpy);
  }

};
