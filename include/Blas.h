#pragma once
#include "Buffer.h"
#include "VkInclude.h"

struct AccelStructureGeometry {
  std::vector<VkAccelerationStructureGeometryTrianglesDataKHR> vertex_data;
  std::vector<VkAccelerationStructureGeometryKHR> as_geometry;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR> offset;
};

struct AccelStructure {
  VkAccelerationStructureKHR accel;
  AllocatedBuffer buffer;

  void create(VkAccelerationStructureCreateInfoKHR& create_info, VmaMemoryUsage mem_usage = VMA_MEMORY_USAGE_GPU_ONLY);
};

struct Blas {
  AccelStructure accel_structure;
  AccelStructureGeometry geometry_info;

  void add_buffers(AllocatedBuffer& vbo, AllocatedBuffer& ibo, u32 max_vertices, u32 max_indices);
  static void build_blas(Blas* blases, u32 count, VkBuildAccelerationStructureFlagsKHR build_flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
};

struct Instance {
  u32 vert_id;
  u32 hit_group_id{0};
  u32 mask{0xFF};
  VkGeometryInstanceFlagsKHR flags {VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR};
  glm::mat4 transform{glm::mat4(1)};

  VkAccelerationStructureInstanceKHR toVkGeometryInstanceKHR(Blas *blas, u32 instance_id) const;
};

struct Tlas {
  AccelStructure accel_structure;
  AllocatedBuffer instance_buffer;
  VkWriteDescriptorSetAccelerationStructureKHR desc_info;
  std::vector<Instance> instances;

  void add_instance(u32 vert_id, u32 hit_group_id, glm::mat4& transform);
  void build_tlas(Blas *blas, u32 count, VkBuildAccelerationStructureFlagsKHR build_flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
  VkWriteDescriptorSetAccelerationStructureKHR* get_desc_info();
};
