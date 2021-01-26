#include "RtProgram.h"
#include <fstream>
#include <iostream>

std::vector<char> readSPIRV(const char* filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  assert_log(file.is_open(), "failed to open file");
  size_t fileSize = (size_t) file.tellg();
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();
  return std::move(buffer);
}

bool createShaderModule(VkShaderModule& shader_module, const char *file, shaderc_shader_kind shader_kind) {
  std::ifstream in(file, std::ios::in | std::ios::binary);
  std::string glsl_src;
  if (!in) {
    err_log("Could not open shader file: {}", file);
    return false;
  }
  glsl_src = std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  auto result = vkcompiler.compiler.CompileGlslToSpv(glsl_src, shader_kind, file, vkcompiler.options);
  if (result.GetCompilationStatus() == shaderc_compilation_status_success) {
    std::vector<u32> spv_src  { result.cbegin(), result.cend() };
    VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    create_info.codeSize = (u32) (spv_src.size()*sizeof(u32));
    create_info.pCode = spv_src.data();
    VK_CHECK(vkCreateShaderModule(vkcontext.device, &create_info, nullptr, &shader_module));
    return true;
  }
  err_log("Shader compilation failed: {}, {}", file, result.GetErrorMessage());
  return false;
}

VkShaderModule createShaderModule(const char* filename) {
  auto code = readSPIRV(filename);
  VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
  VkShaderModule shader_module;
  VK_CHECK(vkCreateShaderModule(vkcontext.device, &createInfo, nullptr, &shader_module));
  return shader_module;
}

void RtProgram::init_shader_groups(const char* rgen, const char* rmiss, const char* rchit, DescSet* sets, u32 count) {
 {
    VkRayTracingShaderGroupCreateInfoKHR rg = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
    rg.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rg.generalShader = 0;
    rg.closestHitShader = VK_SHADER_UNUSED_KHR;
    rg.anyHitShader = VK_SHADER_UNUSED_KHR;
    rg.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.emplace_back(rg);

    VkRayTracingShaderGroupCreateInfoKHR mg = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
    mg.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    mg.generalShader = 1;
    mg.closestHitShader = VK_SHADER_UNUSED_KHR;
    mg.anyHitShader = VK_SHADER_UNUSED_KHR;
    mg.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.emplace_back(mg);
  
    VkRayTracingShaderGroupCreateInfoKHR hg = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
    hg.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    hg.generalShader = VK_SHADER_UNUSED_KHR;
    hg.closestHitShader = 2;
    hg.anyHitShader = VK_SHADER_UNUSED_KHR;
    hg.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.emplace_back(hg);

    bool result = true;

    VkShaderModule raygenSM;
    result = createShaderModule(raygenSM, rgen, shaderc_raygen_shader) && result;
    VkPipelineShaderStageCreateInfo raygenShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    raygenShaderStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    raygenShaderStageInfo.module = raygenSM;
    raygenShaderStageInfo.pName = "main";
    shader_stages.emplace_back(raygenShaderStageInfo);

    VkShaderModule missSM;
    result = createShaderModule(missSM, rmiss, shaderc_miss_shader) && result;
    VkPipelineShaderStageCreateInfo missShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    missShaderStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missShaderStageInfo.module = missSM;
    missShaderStageInfo.pName = "main";
    shader_stages.emplace_back(missShaderStageInfo);

    VkShaderModule chSM;
    result = createShaderModule(chSM, rchit, shaderc_closesthit_shader) && result;
    VkPipelineShaderStageCreateInfo chShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    chShaderStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    chShaderStageInfo.module = chSM;
    chShaderStageInfo.pName = "main";
    shader_stages.emplace_back(chShaderStageInfo);

    if(!result) {
      err_log("Some shaders have not compiled successfully");
      assert(0);
    } else {
      info_log("Shaders compiled successfully!");
    }
    auto desc_layouts = DescSet::get_pl_layouts(sets, count);
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(RtConfig);

    VkPipelineLayoutCreateInfo pl_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pl_info.setLayoutCount = (u32) desc_layouts.size();
    pl_info.pSetLayouts = desc_layouts.data();
    pl_info.pushConstantRangeCount = 1;
    pl_info.pPushConstantRanges = &push_constant_range;

    VK_CHECK(vkCreatePipelineLayout(vkcontext.device, &pl_info, nullptr, &pl_layout));

    VkPipelineLibraryCreateInfoKHR lib_info = { VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR };
    lib_info.libraryCount = 0;
    lib_info.pLibraries = nullptr;
 
    VkRayTracingPipelineCreateInfoKHR pipeline_info = { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
    pipeline_info.stageCount = (u32) shader_stages.size();
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.groupCount = (u32) shader_groups.size();
    pipeline_info.pGroups = shader_groups.data();
    pipeline_info.maxPipelineRayRecursionDepth = vkcontext.device_props.rt_properties.maxRayRecursionDepth-1;
    pipeline_info.layout = pl_layout;
    pipeline_info.pLibraryInfo = nullptr;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(vkcontext.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));
  }
}

void RtProgram::create_sbt() {
  u32 groupCount = rt_shaders.group_count;
  u32 groupHandleSize = vkcontext.device_props.rt_properties.shaderGroupHandleSize;
  u32 baseAlignment = vkcontext.device_props.rt_properties.shaderGroupBaseAlignment;

  u32 sbtSize = groupCount * baseAlignment;
  std::vector<u8> shaderHandleStorage(sbtSize);
  vkGetRayTracingShaderGroupHandlesKHR(vkcontext.device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data());

  sbt_buffer.create(sbtSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

  void* mapped = sbt_buffer.map();
  auto* pData = reinterpret_cast<u8*>(mapped);
  for (u32 i = 0; i < groupCount; ++i) {
      memcpy(pData, shaderHandleStorage.data() + i * groupHandleSize, groupHandleSize);
      pData += baseAlignment;
  }
  sbt_buffer.unmap();  

  
  VkDeviceSize progSize = baseAlignment;
  VkDeviceSize rayGenOffset = 0u * progSize;
  VkDeviceSize missOffset = 1u * progSize;
  VkDeviceSize hitGroupOffset = 2u * progSize;

  VkDeviceAddress sbt_addr = sbt_buffer.get_device_addr();

  rt_shaders.sbt_raygen = {
    .deviceAddress = sbt_addr+rayGenOffset,
    .stride = progSize,
    .size = progSize,
  };

  rt_shaders.sbt_miss = {
    .deviceAddress = sbt_addr+missOffset,
    .stride = progSize,
    .size = progSize,
  };

  rt_shaders.sbt_rchit = {
    .deviceAddress = sbt_addr+hitGroupOffset,
    .stride = progSize,
    .size = progSize,
  };
}

void RtProgram::bind(VkCommandBuffer cmd_buff) {
  vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
}

void RtProgram::render_to_swapchain(const FrameData& frame_data, AllocatedImage& output_image) {
  vkCmdTraceRaysKHR(frame_data.cmd_buff, &rt_shaders.sbt_raygen, &rt_shaders.sbt_miss, &rt_shaders.sbt_rchit, &rt_shaders.sbt_call, 1920, 1080, 1);
  vkCmdPipelineBarrier(frame_data.cmd_buff, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);
  VkImageCopy swapchain_copy = {
    .srcSubresource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .srcOffset { 0, 0, 0 },
    .dstSubresource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .dstOffset { 0, 0, 0 },
    .extent { 1920, 1080, 1 },
  };
  VkImage& render_image = vkcontext.swapchain.images[vkcontext.swapchain.image_index].image;
  vkutil::TransImageLayout(render_image, frame_data.cmd_buff, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkutil::TransImageLayout(output_image.image, frame_data.cmd_buff, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  output_image.cmdCopyImage(frame_data.cmd_buff, render_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &swapchain_copy);
  vkutil::TransImageLayout(output_image.image, frame_data.cmd_buff, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
  vkutil::TransImageLayout(render_image, frame_data.cmd_buff, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void RtProgram::update_shaders(const char *rgen, const char *rmiss, const char *rchit) {
  bool success = true;
  if (rgen != nullptr) {
    VkShaderModule rgen_sm{};
    bool result = createShaderModule(rgen_sm, rgen, shaderc_raygen_shader);
    success = result && success;
    if (result) shader_stages[0].module = rgen_sm;
  }

  if (rmiss != nullptr) {
    VkShaderModule rmiss_sm{};
    bool result = createShaderModule(rmiss_sm, rmiss, shaderc_miss_shader);
    success = result && success;
    if (result) shader_stages[1].module = rmiss_sm;
  }

  if (rchit != nullptr) {
    VkShaderModule rchit_sm{};
    bool result = createShaderModule(rchit_sm, rchit, shaderc_closesthit_shader);
    success = result && success;
    if (result) shader_stages[2].module = rchit_sm;
  }
  if (!success) return;
  VkPipelineLibraryCreateInfoKHR lib_info = { VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR };
  lib_info.libraryCount = 0;
  lib_info.pLibraries = nullptr;
 
  VkRayTracingPipelineCreateInfoKHR pipeline_info = { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
  pipeline_info.stageCount = (u32) shader_stages.size();
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.groupCount = (u32) shader_groups.size();
  pipeline_info.pGroups = shader_groups.data();
  pipeline_info.maxPipelineRayRecursionDepth = vkcontext.device_props.rt_properties.maxRayRecursionDepth-1;
  pipeline_info.layout = pl_layout;
  pipeline_info.pLibraryInfo = nullptr;

  VK_CHECK(vkWaitForFences(vkcontext.device, 1, &vkcontext.frame_data[vkcontext.swapchain.image_index].render_fence, VK_TRUE, UINT64_MAX));
  vkDestroyPipeline(vkcontext.device, pipeline, nullptr);
  VK_CHECK(vkCreateRayTracingPipelinesKHR(vkcontext.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));
  create_sbt();
  info_log("compiled shaders...");
}
