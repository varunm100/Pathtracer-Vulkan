#include "Context.h"
#include "Image.h"
#include "Buffer.h"

void AllocatedImage::create(VkImageUsageFlags image_usage, VkExtent3D extent, u32 mipmap_count) {
  VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  image_info.flags = 0;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.extent = extent;
  image_info.mipLevels = mipmap_count;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = image_usage;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VK_CHECK(vmaCreateImage(vkallocator, &image_info, &alloc_info, &image, &allocation, nullptr));

  VkImageViewCreateInfo image_view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  image_view_info.image = image;
  image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_view_info.subresourceRange.levelCount = mipmap_count;
  image_view_info.subresourceRange.baseArrayLayer = 0;
  image_view_info.subresourceRange.layerCount = 1;
  image_view_info.subresourceRange.baseMipLevel = 0;
  image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

  VK_CHECK(vkCreateImageView(vkcontext.device, &image_view_info, nullptr, &view));

  VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.anisotropyEnable = VK_TRUE;
  sampler_info.maxAnisotropy = 16.0f;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_info.mipLodBias = 0.0f;
  sampler_info.minLod = 0.0f;
  sampler_info.maxLod = 0.0f;

  VK_CHECK(vkCreateSampler(vkcontext.device, &sampler_info, nullptr, &sampler));
}

VkDescriptorImageInfo AllocatedImage::get_desc_info(VkImageLayout image_layout) {
  VkDescriptorImageInfo image_info = {};
  image_info.imageLayout = image_layout;
  image_info.imageView = view;
  image_info.sampler = sampler;

  return std::move(image_info);
}

VkAccessFlags accessFlagsForImageLayout(VkImageLayout layout) {
  switch(layout) {
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_ACCESS_HOST_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT;
    default:
      return VkAccessFlags();
  }
}

VkPipelineStageFlags pipelineStageForLayout(VkImageLayout layout) {
  switch(layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_PIPELINE_STAGE_HOST_BIT;
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    default:
      return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  }
}

void AllocatedImage::cmdTransitionLayout(VkCommandBuffer cmd_buff, VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier image_barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  image_barrier.oldLayout = old_layout;
  image_barrier.newLayout = new_layout;
  image_barrier.image = image;
  image_barrier.srcAccessMask = accessFlagsForImageLayout(old_layout);
  image_barrier.dstAccessMask = accessFlagsForImageLayout(new_layout);
  image_barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, };
  VkPipelineStageFlags src_stage = pipelineStageForLayout(old_layout);
  VkPipelineStageFlags dst_stage = pipelineStageForLayout(new_layout);
  vkCmdPipelineBarrier(cmd_buff, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
}


void AllocatedImage::cmdCopyImage(VkCommandBuffer cmd_buff, VkImage dst_image,VkImageLayout src_layout,VkImageLayout dst_layout, u32 copy_count, VkImageCopy* regions) {
  vkCmdCopyImage(cmd_buff, image, src_layout, dst_image, dst_layout, copy_count, regions);
}

namespace vkutil {

void TransImageLayout(VkImage image, VkCommandBuffer cmd_buff, VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier image_barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  image_barrier.oldLayout = old_layout;
  image_barrier.newLayout = new_layout;
  image_barrier.image = image;
  image_barrier.srcAccessMask = accessFlagsForImageLayout(old_layout);
  image_barrier.dstAccessMask = accessFlagsForImageLayout(new_layout);
  image_barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, };
  VkPipelineStageFlags src_stage = pipelineStageForLayout(old_layout);
  VkPipelineStageFlags dst_stage = pipelineStageForLayout(new_layout);
  vkCmdPipelineBarrier(cmd_buff, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
}

  void toImage(VkCommandBuffer cmd, VkImage image, VkDeviceSize offset, VkDeviceSize size, const void *data, VkExtent3D image_extent) {
    if (!size || !image)
      return;

    AllocatedBuffer staging;
    staging.create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* mapping = staging.map();
    memcpy(mapping, data, size);
    staging.unmap();
    VkBufferImageCopy cpy {};
    cpy.bufferOffset = offset;
    cpy.bufferRowLength = 0;
    cpy.bufferImageHeight = 0;
    cpy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpy.imageSubresource.mipLevel = 0;
    cpy.imageSubresource.baseArrayLayer = 0;
    cpy.imageSubresource.layerCount = 1;
    cpy.imageExtent = image_extent;

    vkCmdCopyBufferToImage(cmd, staging.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);
  }
}
