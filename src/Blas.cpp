#include "Context.h"
#include "Blas.h"
#include "CmdUtils.h"
#include "Scene.h"

void AccelStructure::create(VkAccelerationStructureCreateInfoKHR &create_info, VmaMemoryUsage mem_usage) {
  buffer.create(create_info.size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  create_info.buffer = buffer.buffer;
  vkCreateAccelerationStructureKHR(vkcontext.device, &create_info, nullptr, &accel);
}

void Blas::add_buffers(AllocatedBuffer& vbo, AllocatedBuffer& ibo, u32 max_vertices, u32 max_indices) {
  VkDeviceAddress vbo_addr = vbo.get_device_addr();
  VkDeviceAddress ibo_addr = ibo.get_device_addr();

  VkAccelerationStructureGeometryTrianglesDataKHR vertex_data = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
  vertex_data.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_data.vertexData.deviceAddress = vbo_addr;
  vertex_data.indexData.deviceAddress = ibo_addr;
  vertex_data.indexType = VK_INDEX_TYPE_UINT32;
  vertex_data.transformData = {};
  vertex_data.vertexStride = {sizeof(Vert)};
  vertex_data.maxVertex = max_vertices;

  geometry_info.vertex_data.emplace_back(vertex_data);

  VkAccelerationStructureGeometryKHR as_geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
  as_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  as_geometry.geometry.triangles = vertex_data;
  as_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

  geometry_info.as_geometry.emplace_back(as_geometry);

  VkAccelerationStructureBuildRangeInfoKHR offset = {};
  offset.firstVertex = 0;
  offset.primitiveCount = max_indices/3;
  offset.primitiveOffset = 0;
  offset.transformOffset = 0;
  geometry_info.offset.emplace_back(offset);

}

void Blas::build_blas(Blas* blases, u32 count, VkBuildAccelerationStructureFlagsKHR build_flags) {
  if(!count) return;

  std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_infos(count);
  for (u32 i = 0; i < count; ++i) {
    build_infos[i] = {};
    build_infos[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_infos[i].flags = build_flags;
    build_infos[i].geometryCount = (u32) blases[i].geometry_info.offset.size();
    build_infos[i].pGeometries = blases[i].geometry_info.as_geometry.data();
    build_infos[i].mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_infos[i].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_infos[i].srcAccelerationStructure = VK_NULL_HANDLE;
  }
  VkDeviceSize max_scratch {0};
  for (u32 i = 0; i < count; ++i) {
    std::vector<u32> max_prim_count(blases[i].geometry_info.offset.size());
    for (u32 j = 0; j < blases[i].geometry_info.offset.size(); ++j)
      max_prim_count[j]=blases[i].geometry_info.offset[j].primitiveCount;

    VkAccelerationStructureBuildSizesInfoKHR size_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(vkcontext.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_infos[i], max_prim_count.data(), &size_info);

    VkAccelerationStructureCreateInfoKHR as_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    as_info.size = size_info.accelerationStructureSize;

    blases[i].accel_structure.create(as_info);
    build_infos[i].dstAccelerationStructure = blases[i].accel_structure.accel;

    max_scratch = std::max(max_scratch, size_info.buildScratchSize);
  }
  
  assert(max_scratch && "scratch memory requirements returned 0");
  AllocatedBuffer scratch_buffer;
  scratch_buffer.create(max_scratch, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

  VkDeviceAddress scratch_addr {scratch_buffer.get_device_addr()};

  std::vector<VkCommandBuffer> cmd_buffers(count);
  vkutil::get_cmd_buffers(cmd_buffers.data(), count);
  for (u32 i = 0; i < count; ++i) {
    VkCommandBuffer& cmd_buffer = cmd_buffers[i];
    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd_buffer, &begin_info));
    build_infos[i].scratchData.deviceAddress = scratch_addr;

    std::vector<const VkAccelerationStructureBuildRangeInfoKHR *> pBuildOffset(blases[i].geometry_info.offset.size());
    for (size_t j = 0; j < blases[i].geometry_info.offset.size(); ++j)
      pBuildOffset[j] = &blases[i].geometry_info.offset[j];

    vkCmdBuildAccelerationStructuresKHR(cmd_buffer, 1, &build_infos[i], pBuildOffset.data());
    VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    VK_CHECK(vkEndCommandBuffer(cmd_buffer));
  }
  VkFence build_done = vkutil::submit_cmd_buffers(cmd_buffers.data(), count);
  VkResult result = vkWaitForFences(vkcontext.device, 1, &build_done, VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS) {
    err_log("vk_error waiting for blases to build: {}", result);
  }
  vkutil::free_cmd_buffers(cmd_buffers.data(), count);
  scratch_buffer.destroy();
}

VkAccelerationStructureInstanceKHR Instance::toVkGeometryInstanceKHR(Blas *blas, u32 instance_id) const {
  VkAccelerationStructureDeviceAddressInfoKHR address_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
  address_info.accelerationStructure = blas[vert_id].accel_structure.accel;
  VkDeviceAddress blas_addr = vkGetAccelerationStructureDeviceAddressKHR(vkcontext.device, &address_info);
  
  VkAccelerationStructureInstanceKHR instance_khr;
  // instance_khr.transform is a 4x3 matrix
  // saving the last row that is anyway always (0, 0, 0, 1)
  glm::mat4 transpose = glm::transpose(transform);
  memcpy(&instance_khr.transform, &transpose[0][0], sizeof(instance_khr.transform));

  instance_khr.instanceCustomIndex = instance_id;
  instance_khr.mask = mask;
  instance_khr.instanceShaderBindingTableRecordOffset = hit_group_id;
  instance_khr.flags = flags;
  instance_khr.accelerationStructureReference = blas_addr;

  return std::move(instance_khr);
}

void Tlas::add_instance(u32 vert_id, u32 hit_group_id, glm::mat4& transform) {
  // vert_id = pretty much same as blas_id
  // hit_group_id = might be related to shader binding group
  Instance instance {
    .vert_id = vert_id,
    .hit_group_id = hit_group_id,
    .transform = transform,
  };
  instances.emplace_back(instance);  
}

void Tlas::build_tlas(Blas *blas, u32 count, VkBuildAccelerationStructureFlagsKHR build_flags) {
  assert_log(count == instances.size(), "given count and instances.size() do not match");

  AllocatedBuffer scratch_buffer;

  std::vector<VkAccelerationStructureInstanceKHR> geometry_instances;
  geometry_instances.reserve(instances.size());
  u32 instance_id = 0;
  // NOTE: the order of instances matter! has to be same order as the geometry buffer on gpu
  for (const auto &inst : instances) {
    geometry_instances.push_back(inst.toVkGeometryInstanceKHR(blas, instance_id++));
  }

  bool update = false; // TODO: take as parameter 
  VkDeviceSize instance_desc_size = instances.size()*sizeof(VkAccelerationStructureInstanceKHR);
  const void* instance_data = geometry_instances.data();

  vkutil::immediate_submit([&](VkCommandBuffer cmd) {
    instance_buffer.create(cmd, instance_desc_size, instance_data, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    VkDeviceAddress instance_addr = instance_buffer.get_device_addr();
    VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);


    VkAccelerationStructureGeometryInstancesDataKHR instances_vk { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instances_vk.arrayOfPointers = VK_FALSE;
    instances_vk.data.deviceAddress = instance_addr;

    VkAccelerationStructureGeometryKHR topASGeometry { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topASGeometry.geometry.instances = instances_vk;

    VkAccelerationStructureBuildGeometryInfoKHR topASInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    topASInfo.flags                     = build_flags;
    topASInfo.geometryCount             = 1;
    topASInfo.pGeometries              = &topASGeometry;
    topASInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    topASInfo.srcAccelerationStructure  = VK_NULL_HANDLE;

    u32 count = (u32)instances.size();
    VkAccelerationStructureBuildSizesInfoKHR size_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(vkcontext.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &topASInfo, &count, &size_info);
    if (update == false) {
      VkAccelerationStructureCreateInfoKHR create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
      create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
      create_info.size = size_info.accelerationStructureSize;
      accel_structure.create(create_info);
    }
    scratch_buffer.create(size_info.buildScratchSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    VkDeviceAddress scratch_addr = scratch_buffer.get_device_addr();
    
    topASInfo.srcAccelerationStructure = update ? accel_structure.accel : VK_NULL_HANDLE;
    topASInfo.dstAccelerationStructure = accel_structure.accel;
    topASInfo.scratchData.deviceAddress = scratch_addr;

    VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{static_cast<uint32_t>(instances.size()), 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &topASInfo, &pBuildOffsetInfo);
  });
  scratch_buffer.destroy();
}

VkWriteDescriptorSetAccelerationStructureKHR* Tlas::get_desc_info() {
  desc_info = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
  desc_info.accelerationStructureCount = 1;
  desc_info.pAccelerationStructures = &accel_structure.accel;
  return &desc_info;
}
