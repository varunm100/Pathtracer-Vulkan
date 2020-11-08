#include "Descriptors.h"
#include "Context.h"

void DescSet::add_binding(u32 binding, VkDescriptorType desc_type, u32 desc_count, VkShaderStageFlags shader_stages, const VkSampler *pImmutableSamplers) {
  VkDescriptorSetLayoutBinding set_binding = {};
  set_binding.binding = binding;
  set_binding.descriptorCount = desc_count;
  set_binding.descriptorType = desc_type;
  set_binding.stageFlags = shader_stages;
  set_binding.pImmutableSamplers = pImmutableSamplers;

  bindings.emplace_back(set_binding);
}

void DescSet::allocate_sets(u32 count, DescSet* sets) {
  std::vector<VkDescriptorSetLayout> layouts(count);
  for (u32 i = 0; i < count; ++i) {
    VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layout_info.bindingCount = 1;
    layout_info.pBindings = sets[i].bindings.data();
    layout_info.bindingCount = (u32) sets[i].bindings.size();
    layout_info.flags = 0;

    VK_CHECK(vkCreateDescriptorSetLayout(vkcontext.device, &layout_info, nullptr, &sets[i].layout));
    layouts[i] = sets[i].layout;
  }

  for (u32 i = 0; i < NUM_FRAMES; ++i) {
    VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    alloc_info.descriptorPool = vkcontext.frame_data[i].desc_pool;
    alloc_info.descriptorSetCount = (u32) layouts.size();
    alloc_info.pSetLayouts = layouts.data();
    
    VK_CHECK(vkAllocateDescriptorSets(vkcontext.device, &alloc_info, (&vkcontext.frame_data[i].desc_sets[0]+DescSet::desc_arr_offset)));
  }

  u32 set_index = 0;
  for (u32 i = desc_arr_offset; i < desc_arr_offset+count; ++i) {
    sets[set_index++].id = i;
  }
  DescSet::desc_arr_offset += count;
}

WriteDescSet DescSet::make_write(VkDescriptorBufferInfo buffer_info, u32 binding, u32 arr_element) {
  for (u32 i = 0; i < bindings.size(); ++i) {
    if (bindings[i].binding == binding) {
      WriteDescSet write_set{{
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[0].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pBufferInfo = &buffer_info,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[1].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pBufferInfo = &buffer_info,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[2].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pBufferInfo = &buffer_info,
          },
      }};
      return std::move(write_set);
    }
  }

  assert(0 && "make_write(), binding not found");
  WriteDescSet empty_set = {};
  return empty_set;
}

WriteDescSet DescSet::make_write(VkWriteDescriptorSetAccelerationStructureKHR as_info, u32 binding, u32 arr_element) {
  for (u32 i = 0; i < bindings.size(); ++i) {
    if (bindings[i].binding == binding) {
      WriteDescSet write_set{{
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .pNext = &as_info,
	    .dstSet = vkcontext.frame_data[0].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .pNext = &as_info,
	    .dstSet = vkcontext.frame_data[1].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .pNext = &as_info,
	    .dstSet = vkcontext.frame_data[2].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
          },
      }};
      return std::move(write_set);
    }
  }

  assert(0 && "make_write(), binding not found");
  WriteDescSet empty_set = {};
  return empty_set;
}

WriteDescSet DescSet::make_write(VkDescriptorImageInfo image_info, u32 binding, u32 arr_element) {
  for (u32 i = 0; i < bindings.size(); ++i) {
    if (bindings[i].binding == binding) {
      WriteDescSet write_set{{
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[0].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pImageInfo = &image_info,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[1].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pImageInfo = &image_info,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[2].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pImageInfo = &image_info,
          },
      }};
      return std::move(write_set);
    }
  }

  assert(0 && "make_write(), binding not found");
  WriteDescSet empty_set = {};
  return empty_set;
}

WriteDescSet DescSet::make_write_array(VkDescriptorBufferInfo* buffer_info, u32 binding, u32 arr_element) {
  for (u32 i = 0; i < bindings.size(); ++i) {
    if (bindings[i].binding == binding) {
      WriteDescSet write_set{{
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[0].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pBufferInfo = buffer_info,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[1].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pBufferInfo = buffer_info,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[2].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pBufferInfo = buffer_info,
          },
      }};
      return std::move(write_set);
    }
  }

  assert(0 && "make_write(), binding not found");
  WriteDescSet empty_set = {};
  return empty_set;
}

WriteDescSet DescSet::make_write_array(VkDescriptorImageInfo* image_info, u32 binding, u32 arr_element) {
  for (u32 i = 0; i < bindings.size(); ++i) {
    if (bindings[i].binding == binding) {
      WriteDescSet write_set{{
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[0].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pImageInfo = image_info,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[1].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pImageInfo = image_info,
          },
          {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = vkcontext.frame_data[2].desc_sets[id],
	    .dstBinding = binding,
	    .dstArrayElement = arr_element,
	    .descriptorCount = bindings[i].descriptorCount,
	    .descriptorType = bindings[i].descriptorType,
	    .pImageInfo = image_info,
          },
      }};
      return std::move(write_set);
    }
  }

  assert(0 && "make_write(), binding not found");
  WriteDescSet empty_set = {};
  return empty_set;
}

void DescSet::update_writes(WriteDescSet* writes, u32 count) {
  std::vector<VkWriteDescriptorSet> desc_writes(count*NUM_FRAMES);
  u32 write_count = 0;
  for (u32 i = 0; i < count; ++i) {
    for (u32 j = 0; j < NUM_FRAMES; ++j) {
      desc_writes[write_count++] = std::move(writes[i].writes[j]);    
    }
  }
  
  vkUpdateDescriptorSets(vkcontext.device, count*NUM_FRAMES, desc_writes.data(), 0, nullptr); // has to copy data to gpu, heavy operation
}

void DescSet::bind_sets(VkCommandBuffer cmd_buff, DescSet *sets, u32 count, VkPipelineLayout pl_layout, u32 starting_binding) {
  std::vector<VkDescriptorSet> desc_sets(count);
  for (u32 i = 0; i < count; ++i) {
    desc_sets[i] = vkcontext.frame_data[vkcontext.swapchain.image_index].desc_sets[sets[i].id];
  }
  vkCmdBindDescriptorSets(cmd_buff, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pl_layout, starting_binding, count, desc_sets.data(), 0, nullptr);
}

std::vector<VkDescriptorSetLayout> DescSet::get_pl_layouts(DescSet* sets, u32 count) {
  std::vector<VkDescriptorSetLayout> layouts(count);
  for (u32 i = 0; i < count; ++i) {
    layouts[i] = sets[i].layout;
  }
  return std::move(layouts);	
}

DescSet DescSet::get_copy() {
  DescSet set{
    .id = id,
    .layout = layout,
  };
  return set;
}
