#include "CmdUtils.h"
#include "Context.h"

static VkCommandPool one_time_pool = VK_NULL_HANDLE;

namespace vkutil {

void init_utils() {
  VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  VK_CHECK(vkCreateCommandPool(vkcontext.device, &pool_info, nullptr, &one_time_pool));
}

void immediate_submit(std::function<void(VkCommandBuffer)> execute_cmds) {
  VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  allocate_info.commandBufferCount = 1;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandPool = one_time_pool;

  VkCommandBuffer cmd_buffer;
  VK_CHECK(vkAllocateCommandBuffers(vkcontext.device, &allocate_info, &cmd_buffer));

  VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(cmd_buffer, &begin_info));
  execute_cmds(cmd_buffer);
  VK_CHECK(vkEndCommandBuffer(cmd_buffer));

  VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buffer;

  VK_CHECK(vkQueueSubmit(vkcontext.graphics_queue, 1, &submit_info, VK_NULL_HANDLE));

  VK_CHECK(vkQueueWaitIdle(vkcontext.graphics_queue));
  vkFreeCommandBuffers(vkcontext.device, one_time_pool, 1, &cmd_buffer);
}

void get_cmd_buffers(VkCommandBuffer *buffers, u32 count) {
  VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  allocate_info.commandBufferCount = count;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandPool = one_time_pool;

  VK_CHECK(vkAllocateCommandBuffers(vkcontext.device, &allocate_info, buffers));
}

VkFence submit_cmd_buffers(VkCommandBuffer *buffers, u32 count) {
  VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submit_info.commandBufferCount = count;
  submit_info.pCommandBuffers = buffers;
  VkFence fence;
  VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  VK_CHECK(vkCreateFence(vkcontext.device, &fence_info, nullptr, &fence));
  VK_CHECK(vkQueueSubmit(vkcontext.graphics_queue, 1, &submit_info, fence));
  return fence;
}

void free_cmd_buffers(VkCommandBuffer *buffers, u32 count) {
  vkFreeCommandBuffers(vkcontext.device, one_time_pool, count, buffers);
}
};
