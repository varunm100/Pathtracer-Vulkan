#pragma once
#include "RenderCommon.h"
#include "VkInclude.h"
#include <stdio.h>

struct WriteDescSet {
  VkWriteDescriptorSet writes[3];
};

struct DescSet {
  inline static u32 desc_arr_offset { 0 };

  u32 id { UINT_MAX };
  VkDescriptorSetLayout layout { VK_NULL_HANDLE };
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  void create(VkDescriptorSetLayoutCreateFlags flags = 0);
  void add_binding(u32 binding, VkDescriptorType desc_type, u32 desc_count, VkShaderStageFlags shader_stages, const VkSampler* pImmutableSamplers=nullptr);
  static void allocate_sets(u32 count, DescSet* sets);

  WriteDescSet make_write(VkDescriptorBufferInfo buffer_info, u32 binding, u32 arr_element=0);
  WriteDescSet make_write(VkDescriptorImageInfo image_info, u32 binding, u32 arr_element=0);
  WriteDescSet make_write(VkWriteDescriptorSetAccelerationStructureKHR as_info, u32 binding, u32 arr_element=0);

  WriteDescSet make_write_array(VkDescriptorBufferInfo* buffer_info, u32 binding, u32 arr_element=0);
  WriteDescSet make_write_array(VkDescriptorImageInfo* image_info, u32 binding, u32 arr_element=0);

  static void update_writes(WriteDescSet* writes, u32 count);
  static void bind_sets(VkCommandBuffer cmd_buff, DescSet *sets, u32 count, VkPipelineLayout pl_layout, u32 starting_binding);
  static std::vector<VkDescriptorSetLayout> get_pl_layouts(DescSet* sets, u32 count);
  DescSet get_copy();
};
