#pragma once
#include "Common.h"
#include "VkInclude.h"

namespace vkcmd {
  
};

namespace vkutil {
  void init_utils();
  void immediate_submit(std::function<void(VkCommandBuffer)> execute_cmds); // blocks thread! use only for initilization
  void get_cmd_buffers(VkCommandBuffer* buffers, u32 count);
  VkFence submit_cmd_buffers(VkCommandBuffer *buffers, u32 count);
  void free_cmd_buffers(VkCommandBuffer *buffers, u32 count);
}
