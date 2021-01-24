#pragma once
#include "VkInclude.h"
#include "Buffer.h"
#include "Image.h"
#include "Descriptors.h"
#include "Context.h"
#include <unordered_set>

struct RtConfig {
  int sample_count;
  int max_bounce;
  float gamma;
  float exposure;
  u32 num_lights;
  u32 frame_count;
  bool show_lights;
};

struct RtShader {
  u32 group_count = 3;
  VkShaderModule raygen;
  VkShaderModule miss;
  VkShaderModule chit;

  VkStridedDeviceAddressRegionKHR sbt_raygen{};
  VkStridedDeviceAddressRegionKHR sbt_miss{};
  VkStridedDeviceAddressRegionKHR sbt_rchit{};
  VkStridedDeviceAddressRegionKHR sbt_call{};
};

struct RtProgram {
  VkPipeline pipeline{VK_NULL_HANDLE};
  VkPipelineLayout pl_layout{VK_NULL_HANDLE};
  AllocatedBuffer sbt_buffer;
  RtShader rt_shaders;
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

  void init_shader_groups(const char* rgen, const char* rmiss, const char* rchit, DescSet* sets, u32 count);
  void create_sbt();
  void bind(VkCommandBuffer cmd_buff);
  void render_to_swapchain(const FrameData& frame_data, AllocatedImage& output_image);
  void update_shaders(const char* rgen=nullptr, const char* rmiss=nullptr, const char* rchit=nullptr);
};
