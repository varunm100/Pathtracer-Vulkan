#include "Context.h"
#include "Buffer.h"
#include "Blas.h"
#include "Descriptors.h"
#include "Image.h"
#include "CmdUtils.h"
#include "Camera.h"
#include <fstream>
#include <glm/gtx/string_cast.hpp>
#include "RtProgram.h"
#include <vulkan/shaderc.h>
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "imgui.h"
#include "Scene.h"

void check_input(GLFWwindow *window, Camera* camera, float dt) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    exit(0);
  }
  camera->check_input(window, dt);
  camera->update_ubo();
}

void run(GLFWwindow* window);

int main() {
  u32 glfw_init = glfwInit();
  assert_log(glfw_init, "glfwInit() failed");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(1920, 1080, "RT Test", nullptr, nullptr);

  VulkanContext::InitContext(window);
  run(window);
}

static RtConfig rt_config { .sample_count = 3, .max_bounce = 5, .gamma = 2.2f, .exposure = 1.0f, .num_lights=1, .frame_count = 0, .show_lights = true, };

void draw_gui(RtProgram& program, Camera* camera) {
  const char* rgen = "../../../shaders/raytrace.rgen";
  const char* rchit = "../../../shaders/raytrace.rchit";
  const char* rmiss = "../../../shaders/raytrace.rmiss";
  ImGui::Begin("Shaders");
  ImGui::Text("Shader Reload");
  if (ImGui::Button("rgen")) {
    program.update_shaders(rgen, nullptr, nullptr);
  }
  if (ImGui::Button("rchit")) {
    program.update_shaders(nullptr, nullptr, rchit);
  }
  if (ImGui::Button("rmiss")) {
    program.update_shaders(nullptr, rmiss, nullptr);    
  }
  ImGui::End();
  ImGui::Begin("RT Config");
  if (ImGui::Button("restart")) {
    rt_config.frame_count = 0;
    camera->frame_count = 0;
  }
  ImGui::Text("Samples: %d", rt_config.frame_count*rt_config.sample_count);
  ImGui::SliderInt("Sample Count", &rt_config.sample_count, 0, 30);
  ImGui::SliderInt("Max Bounce", &rt_config.max_bounce, 0, vkcontext.device_props.rt_properties.maxRecursionDepth);
  ImGui::SliderFloat("Gamma", &rt_config.gamma, 0, 5);
  ImGui::SliderFloat("Exposure", &rt_config.exposure, 0.0f, 1.0f);
  ImGui::Checkbox("Show Lights", &rt_config.show_lights);
  ImGui::End();
}

void run(GLFWwindow* window) {
  Scene scene;
  std::string filename = "../../../scenes/cornell_box.scene";
  scene.Load_Scene(filename);
  scene.Build_Structures();
  rt_config.num_lights = scene.lights.size();
  info_log("-- Loaded Scene --");

  AllocatedImage output_image;
  AllocatedImage progressive;
  output_image.create(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, {1920, 1080, 1});
  progressive.create(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, {1920, 1080, 1});
  vkutil::immediate_submit([&](VkCommandBuffer buffer) {
    output_image.cmdTransitionLayout(buffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    progressive.cmdTransitionLayout(buffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  });

  DescSet global_set;
  global_set.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR); // camera
  global_set.add_binding(1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // tlas
  global_set.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR); // output image
  global_set.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR); // progressive image

  DescSet::allocate_sets(1, &global_set);

  WriteDescSet writes[] = {
    global_set.make_write(scene.camera->ubo.get_desc_info(0, sizeof(CameraData)), 0),
    global_set.make_write(scene.tlas.get_desc_info(), 1),
    global_set.make_write(output_image.get_desc_info(VK_IMAGE_LAYOUT_GENERAL), 2),
    global_set.make_write(progressive.get_desc_info(VK_IMAGE_LAYOUT_GENERAL), 3),
  };
  DescSet::update_writes(writes, COUNT_OF(writes));
  
  DescSet sets[] = { global_set, scene.scene_set };
  RtProgram rt_program;
  rt_program.init_shader_groups("../../../shaders/raytrace.rgen",
				"../../../shaders/raytrace.rmiss",
				"../../../shaders/raytrace.rchit", sets, COUNT_OF(sets));
  rt_program.create_sbt();

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  glfwSetWindowUserPointer(window, scene.camera);
  glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
    Camera* tcam = (Camera*)glfwGetWindowUserPointer(window);
    tcam->mouse_callback(window, xpos, ypos);
  });
  glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
    Camera* tcam = (Camera*)glfwGetWindowUserPointer(window);
    if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
      int mode = tcam->focused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
      tcam->focused = 1 - tcam->focused;
      glfwSetInputMode(window, GLFW_CURSOR, mode);
    }
  });
  scene.camera->data = scene.camera->ubo.map();

  double prevTime = glfwGetTime();
  float dt = 0;
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    
    dt = (float)(glfwGetTime() - prevTime);
    if(scene.camera->frame_count <= 1e9) ++scene.camera->frame_count;
    check_input(window, scene.camera, dt);
    rt_config.frame_count = scene.camera->frame_count;

    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    draw_gui(rt_program, scene.camera);
    ImGui::Render();
    VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    begin_info.clearValueCount = 0;
    begin_info.renderPass = vkcontext.swapchain.render_pass;
    begin_info.renderArea.offset = { 0, 0 };
    begin_info.renderArea.extent = { 1920, 1080 };

    auto& frame_data = vkcontext.StartFrame();
    rt_program.bind(frame_data.cmd_buff);
    DescSet sets[] = {global_set.get_copy(), scene.scene_set.get_copy(), };
    DescSet::bind_sets(frame_data.cmd_buff, sets, COUNT_OF(sets), rt_program.pl_layout, 0);
    vkCmdPushConstants(frame_data.cmd_buff, rt_program.pl_layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(RtConfig), &rt_config);
    rt_program.render_to_swapchain(frame_data, output_image);

    begin_info.framebuffer = vkcontext.swapchain.images[vkcontext.swapchain.image_index].fbo;
    vkCmdBeginRenderPass(frame_data.cmd_buff, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame_data.cmd_buff);
    vkCmdEndRenderPass(frame_data.cmd_buff);
    vkcontext.EndFrame();
  }
}
