#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION

#include "Context.h"
#include <GLFW/glfw3.h>
#include "CmdUtils.h"
#include "Image.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "RtProgram.h"

VkContext vkcontext;
VmaAllocator vkallocator;
Compiler vkcompiler;

void VulkanContext::init_instance() {
  VK_CHECK(volkInitialize());

  VkApplicationInfo application_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
  application_info.apiVersion = VK_API_VERSION_1_2;
  application_info.pApplicationName = "Raytracing Test";

  const char* enabled_layers[] = {
    #ifdef VALIDATION_LAYERS
    "VK_LAYER_KHRONOS_validation",
    #endif
    "VK_LAYER_LUNARG_monitor",
  };

  u32 extension_count;
  glfwGetRequiredInstanceExtensions(&extension_count);
  const char** required_extensions = glfwGetRequiredInstanceExtensions(&extension_count);

  std::vector<const char*> instance_extensions(required_extensions, required_extensions+extension_count);
  instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  VkInstanceCreateInfo instance_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
  instance_info.enabledExtensionCount = (u32) instance_extensions.size();
  instance_info.ppEnabledExtensionNames = instance_extensions.data();
  instance_info.enabledLayerCount = COUNT_OF(enabled_layers);
  instance_info.ppEnabledLayerNames = enabled_layers;
  instance_info.pApplicationInfo = &application_info;

  VK_CHECK(vkCreateInstance(&instance_info, nullptr, &vkcontext.instance));
  volkLoadInstance(vkcontext.instance);
  info_log("Initialized instance");
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
        void* pUserData) {
  err_log("Validation layer: {}", pCallbackData->pMessage);
  return VK_FALSE;
}

void VulkanContext::init_debug_utils() {
    VkDebugUtilsMessengerEXT debug_messenger;
    VkDebugUtilsMessengerCreateInfoEXT debug_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    debug_info.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | */VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.pfnUserCallback = debugCallback;
    //    debug_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(vkcontext.instance, &debug_info, nullptr, &debug_messenger));
    info_log("Initialized DebugUtils");
}

void VulkanContext::init_device() {
  // pick physical device
  {
    u32 count;
    VK_CHECK(vkEnumeratePhysicalDevices(vkcontext.instance, &count, nullptr));
    std::vector<VkPhysicalDevice> phys_devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(vkcontext.instance, &count, phys_devices.data()));
    assert_log(count > 0, "vkEnumeratePhysicalDevices returned 0");

    bool found = false;
    for (const auto &device : phys_devices) {
      VkPhysicalDeviceFeatures device_features;
      vkGetPhysicalDeviceFeatures(device, &device_features);
      VkPhysicalDeviceProperties2 phys_device_prop = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
      VkPhysicalDeviceRayTracingPropertiesKHR rt_properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_KHR };
      rt_properties.pNext = nullptr;
      phys_device_prop.pNext = &rt_properties;
      vkGetPhysicalDeviceProperties2(device, &phys_device_prop);
      if(device_features.samplerAnisotropy && phys_device_prop.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && !found) {
	info_log("Device Selected: {}", phys_device_prop.properties.deviceName);
	info_log("Api Version: {}.{}.{}"
		 , VK_VERSION_MAJOR(phys_device_prop.properties.apiVersion)
		 , VK_VERSION_MINOR(phys_device_prop.properties.apiVersion)
		 , VK_VERSION_PATCH(phys_device_prop.properties.apiVersion));

	info_log("Driver Version: {}.{}.{}"
		 , VK_VERSION_MAJOR(phys_device_prop.properties.driverVersion)
		 , VK_VERSION_MINOR(phys_device_prop.properties.driverVersion)
		 , VK_VERSION_PATCH(phys_device_prop.properties.driverVersion));

	vkcontext.phys_device = device;
	vkcontext.device_props.rt_properties = rt_properties;
	found = true;
      }
    }
    assert(found && "Could not find a physical device with raytracing support");
  }
  // create vulkan device
  {
    u32 family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(vkcontext.phys_device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> device_queues(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(vkcontext.phys_device, &family_count, device_queues.data());

    u32 queue_index = 0;
    for (const auto& queue : device_queues) {
      if (queue.queueFlags & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT )) break;
      ++queue_index;
    }
    vkcontext.device_props.graphics_index = queue_index;

    float queue_prios[] = { 1.0f };
    VkDeviceQueueCreateInfo graphics_queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    graphics_queue_info.queueFamilyIndex = vkcontext.device_props.graphics_index;
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = queue_prios;

    std::vector<VkDeviceQueueCreateInfo> queue_infos(1);
    queue_infos[0] = graphics_queue_info;

    const char* deviceExtensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        
      VK_KHR_RAY_TRACING_EXTENSION_NAME,
      VK_KHR_MAINTENANCE3_EXTENSION_NAME,
      VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
      VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
      VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };
    
    VkPhysicalDeviceDescriptorIndexingFeatures descIndexingFeature = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
    descIndexingFeature.runtimeDescriptorArray = VK_TRUE;
    descIndexingFeature.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    descIndexingFeature.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descIndexingFeature.pNext = nullptr;

    VkPhysicalDeviceRayTracingFeaturesKHR raytracingFeature = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_FEATURES_KHR };
    raytracingFeature.rayTracing = VK_TRUE;
    raytracingFeature.pNext = &descIndexingFeature;

    VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeature = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES };
    scalarBlockLayoutFeature.scalarBlockLayout = VK_TRUE;
    scalarBlockLayoutFeature.pNext = &raytracingFeature;

    VkPhysicalDeviceBufferDeviceAddressFeatures deviceaddressFeature = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    deviceaddressFeature.pNext = &scalarBlockLayoutFeature;
    deviceaddressFeature.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    deviceFeatures.pNext = &deviceaddressFeature;
    deviceFeatures.features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceInfo.enabledExtensionCount = COUNT_OF(deviceExtensions);
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;
    deviceInfo.queueCreateInfoCount = (u32) queue_infos.size();
    deviceInfo.pQueueCreateInfos = queue_infos.data();
    deviceInfo.pNext = &deviceFeatures;

    VK_CHECK(vkCreateDevice(vkcontext.phys_device, &deviceInfo, nullptr, &vkcontext.device));
    volkLoadDevice(vkcontext.device);
    info_log("Initialized logical device");

    vkGetDeviceQueue(vkcontext.device, vkcontext.device_props.graphics_index, 0, &vkcontext.graphics_queue);
    vkcontext.present_queue = vkcontext.graphics_queue;
    vkcontext.transfer_queue = vkcontext.graphics_queue;
  }
}

void VulkanContext::init_allocator() {
  VmaAllocatorCreateInfo allocator_info = {};
  allocator_info.physicalDevice = vkcontext.phys_device;
  allocator_info.device = vkcontext.device;
  allocator_info.instance = vkcontext.instance;
  allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VK_CHECK(vmaCreateAllocator(&allocator_info, &vkallocator));
}

void Swapchain::init(GLFWwindow* window) {
  int width, height;
  VkSurfaceCapabilitiesKHR surface_caps;
  {
    glfwGetWindowSize(window, &width, &height);
    glfwCreateWindowSurface(vkcontext.instance, window, nullptr, &surface);

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext.phys_device, surface, &surface_caps));
    VkBool32 supports_present = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(vkcontext.phys_device, vkcontext.device_props.graphics_index, surface,  &supports_present);
    assert_log(supports_present, "no queue supports present and graphics!");
  }

  {
    VkSwapchainCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    create_info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.presentMode = vkcontext.device_props.present_mode;
    create_info.imageExtent = { (u32) (width), (u32) (height) };
    create_info.minImageCount = NUM_FRAMES;
    create_info.imageArrayLayers = 1;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = surface_caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create_info.surface = surface;
    VK_CHECK(vkCreateSwapchainKHR(vkcontext.device, &create_info, nullptr, &swapchain));
  }

  {
    u32 image_count;
    VkImage swapchain_images[NUM_FRAMES];
    vkGetSwapchainImagesKHR(vkcontext.device, swapchain, &image_count, nullptr);
    assert_log(image_count == NUM_FRAMES, "image_count is not equal to NUM_FRAMES");
    vkGetSwapchainImagesKHR(vkcontext.device, swapchain, &image_count, swapchain_images);
    for (u32 i = 0; i < NUM_FRAMES; ++i) {
      images[i].image = swapchain_images[i];
      VkImageViewCreateInfo create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      create_info.image = images[i].image;
      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      create_info.format = vkcontext.device_props.swapchain_format;
      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;
      VK_CHECK(vkCreateImageView(vkcontext.device, &create_info, nullptr, &images[i].view));
    }
  }

  {
    VkAttachmentDescription color_attachment {};
    color_attachment.format = vkcontext.device_props.swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(vkcontext.device, &render_pass_info, nullptr, &render_pass));
  }

  {
    VkFramebufferCreateInfo fb_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    fb_info.renderPass = render_pass;
    fb_info.width = 1920;
    fb_info.height = 1080;
    fb_info.layers = 1;

    for (int i = 0; i < NUM_FRAMES; ++i) {
      fb_info.attachmentCount = 1;
      fb_info.pAttachments = &images[i].view;
      VK_CHECK(vkCreateFramebuffer(vkcontext.device, &fb_info, nullptr, &images[i].fbo));
    }
  }
}

void VulkanContext::init_swapchain(GLFWwindow* window) {
  vkcontext.swapchain.init(window);
  init_cmd_pools();
  vkutil::init_utils();
  info_log("Initialized swapchain and cmd pools");
  vkutil::immediate_submit([](VkCommandBuffer cmd_buffer) {
    for (u32 i = 0; i < NUM_FRAMES; ++i) {
      vkutil::TransImageLayout(vkcontext.swapchain.images[i].image, cmd_buffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
  });
}

void VulkanContext::init_cmd_pools() {
  VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = vkcontext.device_props.graphics_index;

  VK_CHECK(vkCreateCommandPool(vkcontext.device, &pool_info, nullptr, &vkcontext.cmd_pool));
 
  VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  alloc_info.commandBufferCount = NUM_FRAMES;
  alloc_info.commandPool = vkcontext.cmd_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  VkCommandBuffer cmd_buffers[NUM_FRAMES];
  VK_CHECK(vkAllocateCommandBuffers(vkcontext.device, &alloc_info, cmd_buffers));
  
  size_t i = 0;  
  for (auto& frame_data : vkcontext.frame_data) frame_data.cmd_buff = cmd_buffers[i++];

}

void VulkanContext::init_desc_pools(){
  VkDescriptorPoolSize sizes[] = {
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_DESC_ACCELERATION_STRUCTURES},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_DESC_UNIFORM_BUFFERS},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_DESC_STORAGE_IMAGES},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_DESC_STORAGE_BUFFERS},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_DESC_COMBINED_IMAGE_SAMPLERS},
  };

  VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pool_info.flags = 0;
  pool_info.maxSets = MAX_DESC_SETS;
  pool_info.poolSizeCount = COUNT_OF(sizes);
  pool_info.pPoolSizes = sizes;

  for (u32 i = 0; i < NUM_FRAMES; ++i) {
    VK_CHECK(vkCreateDescriptorPool(vkcontext.device, &pool_info, nullptr, &vkcontext.frame_data[i].desc_pool));
  }
  info_log("Initialized descriptor pools");
}

void VulkanContext::init_compiler() {
  vkcompiler.options.SetIncluder(std::make_unique<FileIncluder>(&vkcompiler.ffinder));
  vkcompiler.options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
  vkcompiler.options.SetTargetSpirv(shaderc_spirv_version_1_5);
  vkcompiler.options.SetSourceLanguage(shaderc_source_language_glsl);
  vkcompiler.options.SetOptimizationLevel(shaderc_optimization_level_performance);
  vkcompiler.ffinder.search_path().push_back("../../../shaders/");
  vkcompiler.ffinder.search_path().push_back("shaders/");
}

void VulkanContext::init_imgui(GLFWwindow* window) {
  VkDescriptorPoolSize pool_sizes[] = {
      { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
      { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (u32) std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  VkDescriptorPool imgui_pool;
  VK_CHECK(vkCreateDescriptorPool(vkcontext.device, &pool_info, nullptr, &imgui_pool));

  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForVulkan(window, true);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = vkcontext.instance;
  init_info.PhysicalDevice = vkcontext.phys_device;
  init_info.Device = vkcontext.device;
  init_info.Queue = vkcontext.graphics_queue;
  init_info.DescriptorPool = imgui_pool;
  init_info.MinImageCount = NUM_FRAMES;
  init_info.ImageCount = NUM_FRAMES;
  init_info.Allocator = nullptr;
  
  ImGui_ImplVulkan_Init(&init_info, vkcontext.swapchain.render_pass);

  vkutil::immediate_submit([](VkCommandBuffer cmd) {
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
  });
  info_log("Initilized ImGui");
}

void VulkanContext::InitContext(GLFWwindow* window) {
  VulkanContext::init_instance();
  #ifdef VALIDATION_LAYERS
  VulkanContext::init_debug_utils();
  #endif
  VulkanContext::init_device();
  VulkanContext::init_allocator();
  VulkanContext::init_swapchain(window);  
  VulkanContext::init_imgui(window);
  VulkanContext::init_desc_pools();
  VulkanContext::init_compiler();

  VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (u32 i = 0; i < NUM_FRAMES; ++i) {
    VK_CHECK(vkCreateSemaphore(vkcontext.device, &semaphore_info, nullptr, &vkcontext.frame_data[i].acquire_semaphore));
    VK_CHECK(vkCreateSemaphore(vkcontext.device, &semaphore_info, nullptr, &vkcontext.frame_data[i].present_semaphore));
    VK_CHECK(vkCreateFence(vkcontext.device, &fence_info, nullptr, &vkcontext.frame_data[i].render_fence));
  }
}

const FrameData& VkContext::StartFrame() {
  const FrameData& next_frame = frame_data[frame_index++];
  frame_index %= NUM_FRAMES;

  VK_CHECK(vkWaitForFences(vkcontext.device, 1, &next_frame.render_fence, VK_TRUE, UINT64_MAX));
  VK_CHECK(vkResetFences(vkcontext.device, 1, &next_frame.render_fence));

  VK_CHECK(vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, next_frame.acquire_semaphore, VK_NULL_HANDLE, &swapchain.image_index));
  const FrameData& curr_frame = frame_data[swapchain.image_index];
  VK_CHECK(vkResetCommandBuffer(curr_frame.cmd_buff, 0));
  VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_CHECK(vkBeginCommandBuffer(curr_frame.cmd_buff, &begin_info));
  return curr_frame;
}

void VkContext::EndFrame() {
  const FrameData& frameData = frame_data[swapchain.image_index];
  VK_CHECK(vkEndCommandBuffer(frameData.cmd_buff));

  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
  VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &frameData.acquire_semaphore;
  submit_info.pWaitDstStageMask = waitStages;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &frameData.present_semaphore;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &frameData.cmd_buff;

  VK_CHECK(vkQueueSubmit(graphics_queue, 1, &submit_info, frameData.render_fence));

  VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &frameData.present_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain.swapchain;
  present_info.pImageIndices = &swapchain.image_index;
  VK_CHECK(vkQueuePresentKHR(present_queue, &present_info));
}
