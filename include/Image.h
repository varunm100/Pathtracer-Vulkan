#pragma once
#include "VkInclude.h"
#include <vk_mem_alloc/vk_mem_alloc.h>
#include "Common.h"
#include "Buffer.h"

struct AllocatedImage {
  VkImage image { VK_NULL_HANDLE };
  VkImageView view { VK_NULL_HANDLE };
  VkSampler sampler { VK_NULL_HANDLE };
  VmaAllocation allocation { VK_NULL_HANDLE };

  void create(VkImageUsageFlags image_usage, VkExtent3D extent, u32 mipmap_count=1);
  VkDescriptorImageInfo get_desc_info(VkImageLayout image_layout);
  void cmdTransitionLayout(VkCommandBuffer cmd_buff, VkImageLayout old_layout, VkImageLayout new_layout);
  void cmdCopyImage(VkCommandBuffer cmd_buff, VkImage dst_image,VkImageLayout src_layout,VkImageLayout dst_layout, u32 copy_count, VkImageCopy* regions);
};

namespace vkutil {  
  void TransImageLayout(VkImage image, VkCommandBuffer cmd_buff, VkImageLayout old_layout, VkImageLayout new_layout);
  void toImage(VkCommandBuffer cmd, VkImage image, VkDeviceSize offset, VkDeviceSize size, const void *data, VkExtent3D image_extent);
};
