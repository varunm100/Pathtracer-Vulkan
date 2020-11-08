#pragma once
#include "VkInclude.h"
#include <vk_mem_alloc/vk_mem_alloc.h>
#include "RenderCommon.h"
#include "vulkan/shaderc.hpp"
#include <GLFW/glfw3.h>
#include "FileIncluder.h"

struct SwapchainImage{
  VkImage image;
  VkImageView view;
  VkFramebuffer fbo;
};

struct Swapchain {
  u32 image_index;
  VkRenderPass render_pass;
  VkSurfaceKHR surface {VK_NULL_HANDLE};
  VkSwapchainKHR swapchain {VK_NULL_HANDLE};
  SwapchainImage images[NUM_FRAMES];
  
  void init(GLFWwindow* window);
};

struct VkDeviceProps {
  u32 graphics_index;
  VkFormat image_format;
  VkFormat swapchain_format {VK_FORMAT_B8G8R8A8_SRGB};
  VkColorSpaceKHR swapchain_colorspace {VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  VkPresentModeKHR present_mode {VK_PRESENT_MODE_FIFO_KHR};
  VkPhysicalDeviceRayTracingPropertiesKHR rt_properties;
};

struct FrameData {
  VkSemaphore acquire_semaphore;
  VkSemaphore present_semaphore;
  VkFence render_fence;
  
  VkCommandBuffer cmd_buff;
  VkDescriptorPool desc_pool;
  VkDescriptorSet desc_sets[MAX_DESC_SETS] { VK_NULL_HANDLE };
};

struct VkContext {
  VkInstance instance;
  VkPhysicalDevice phys_device;
  VkDevice device;
  VkQueue graphics_queue, present_queue, transfer_queue;
  VkCommandPool cmd_pool;
  VkDeviceProps device_props;

  u32 frame_index = 0;
  Swapchain swapchain;
  FrameData frame_data[NUM_FRAMES];
  
  const FrameData& StartFrame();
  void EndFrame();
};

struct Compiler {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  FileFinder ffinder{};
};

extern VkContext vkcontext;
extern VmaAllocator vkallocator;
extern Compiler vkcompiler;

namespace VulkanContext {
  void init_instance();
  void init_debug_utils();
  void init_device();
  void init_allocator();
  void init_swapchain(GLFWwindow* window);
  void init_cmd_pools();
  void init_desc_pools();
  void init_compiler();
  void init_imgui(GLFWwindow* window);

  void InitContext(GLFWwindow* window);
};
