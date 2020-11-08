#include "Context.h"
#include "Buffer.h"
#include "Blas.h"
#include "Descriptors.h"
#include "Image.h"
#include "CmdUtils.h"
#include "Camera.h"
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include <glm/gtx/string_cast.hpp>
#include "RtProgram.h"
#include <vulkan/shaderc.h>
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "imgui.h"

void check_input(GLFWwindow *window, Camera& camera, float dt) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    exit(0);
  }
  camera.check_input(window, dt);
  camera.update_ubo();
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

static RtConfig rt_config { .sample_count = 3, .max_bounce = 5, .gamma = 2.2f, .exposure = 0.5f };

void draw_gui(RtProgram& program) {
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
  ImGui::SliderInt("Sample Count", &rt_config.sample_count, 0, 30);
  ImGui::SliderInt("Max Bounce", &rt_config.max_bounce, 0, vkcontext.device_props.rt_properties.maxRecursionDepth);
  ImGui::SliderFloat("Gamma", &rt_config.gamma, 0, 5);
  ImGui::SliderFloat("Exposure", &rt_config.exposure, 0.0f, 1.0f);
  ImGui::End();
}

bool createImageFromFile(const char** files, AllocatedImage* images, u32 count, bool vert_flip) {
  bool success = true;
  vkutil::immediate_submit([&](VkCommandBuffer cmd_buff) {
    for (u32 i = 0; i < count; ++i) {
      const char* file = files[i];
      AllocatedImage& image = images[i];
    
      int width, height, channels;
      stbi_uc* pixels = stbi_load(file, &width, &height, &channels, STBI_rgb_alpha);
      stbi_set_flip_vertically_on_load(vert_flip);
      if (!pixels) {
	err_log("Failed to load image: {}", file);
	success = false;
	return;
      } else {
	image.create(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, { (u32)width, (u32)height, 1});
	image.cmdTransitionLayout(cmd_buff, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkutil::toImage(cmd_buff, image.image, 0, width * height * 4, pixels, {(u32)width, (u32)height, 1});
	image.cmdTransitionLayout(cmd_buff, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      }
      stbi_image_free(pixels);
    }
  });
  return success;
}

static void createSphere(AllocatedBuffer &vbo, AllocatedBuffer &ibo, VkCommandBuffer cmd, u32& vert_count, u32& index_count, u32 tex_id) {
  const int slices = 32;
  const int stacks = 16;
	
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  const float pi = 3.14159265358979f;
  glm::vec3 center(0, 0, 0);
  float radius = 1;
  for (int j = 0; j <= stacks; ++j) {
    const float j0 = pi * j / stacks;

    const float v = radius * -std::sin(j0);
    const float z = radius * std::cos(j0);
		
    const float n0 = -std::sin(j0);
    const float n1 = std::cos(j0);

    for (int i = 0; i <= slices; ++i) {
      const float i0 = 2 * pi * i / slices;

      const glm::vec3 position(center.x + v * std::sin(i0),
			       center.y + z,
			       center.z + v * std::cos(i0));
			
      const glm::vec3 normal(n0 * std::sin(i0),
			     n1,
			     n0 * std::cos(i0));

      const glm::vec2 texCoord(static_cast<float>(i) / slices,
			       static_cast<float>(j) / stacks);

      vertices.push_back(Vertex{ position, normal, texCoord, tex_id });
    }
  }

  for (int j = 0; j < stacks; ++j) {
    for (int i = 0; i < slices; ++i){
      const auto j0 = (j + 0) * (slices + 1);
      const auto j1 = (j + 1) * (slices + 1);
      const auto i0 = i + 0;
      const auto i1 = i + 1;
			
      indices.push_back(j0 + i0);
      indices.push_back(j1 + i0);
      indices.push_back(j1 + i1);
			
      indices.push_back(j0 + i0);
      indices.push_back(j1 + i1);
      indices.push_back(j0 + i1);
    }
  }
  vbo.create(cmd, vertices.size()*sizeof(Vertex), vertices.data(),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  ibo.create(cmd, indices.size()*sizeof(u32), indices.data(),VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  vert_count = vertices.size();
  index_count = indices.size();
}

void run(GLFWwindow* window) {
  AllocatedBuffer plane_vbo;
  AllocatedBuffer plane_ibo;

  AllocatedBuffer cube_vbo;
  AllocatedBuffer cube_ibo;

  AllocatedBuffer sphere_vbo;
  AllocatedBuffer sphere_ibo;
  u32 sphere_v_count, sphere_i_count;
  AllocatedBuffer scene_buff;
  AllocatedBuffer mat_buff;
  AllocatedImage output_image;
  output_image.create(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, {1920, 1080, 1});
  
  std::vector<SceneGeometry> scene;
  std::vector<Material> materials;
  u32 vert_count;
  float plane_length = 2.5f;
  vkutil::immediate_submit([&](VkCommandBuffer buffer) {
    Vertex vertices[] = {
      {    -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0 },
      {     0.5f, -0.5f, -0.5f,   0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0 },
      {     0.5f,  0.5f, -0.5f,   0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0 },
      {     0.5f,  0.5f, -0.5f,   0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0 },
      {    -0.5f,  0.5f, -0.5f,   0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0 },
      {    -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0 },

      {    -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0 },
      {     0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0 },
      {     0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0 },
      {     0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0 },
      {    -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0 },
      {    -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0 },

      {    -0.5f,  0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0 },
      {    -0.5f,  0.5f, -0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0 },
      {    -0.5f, -0.5f, -0.5f,  -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0 },
      {    -0.5f, -0.5f, -0.5f,  -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0 },
      {    -0.5f, -0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 },
      {    -0.5f,  0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0 },

      {     0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0 },
      {     0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0 },
      {     0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0 },
      {     0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0 },
      {     0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 },
      {     0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0 },

      {    -0.5f, -0.5f, -0.5f,   0.0f,-1.0f, 0.0f, 0.0f, 1.0f, 0 },
      {     0.5f, -0.5f, -0.5f,   0.0f,-1.0f, 0.0f, 1.0f, 1.0f, 0 },
      {     0.5f, -0.5f,  0.5f,   0.0f,-1.0f, 0.0f, 1.0f, 0.0f, 0 },
      {     0.5f, -0.5f,  0.5f,   0.0f,-1.0f, 0.0f, 1.0f, 0.0f, 0 },
      {    -0.5f, -0.5f,  0.5f,   0.0f,-1.0f, 0.0f, 0.0f, 0.0f, 0 },
      {    -0.5f, -0.5f, -0.5f,   0.0f,-1.0f, 0.0f, 0.0f, 1.0f, 0 },

      {    -0.5f,  0.5f, -0.5f,   0.0f,1.0f, 0.0f, 0.0f, 1.0f, 0 },
      {     0.5f,  0.5f, -0.5f,   0.0f,1.0f, 0.0f, 1.0f, 1.0f, 0 },
      {     0.5f,  0.5f,  0.5f,   0.0f,1.0f, 0.0f, 1.0f, 0.0f, 0 },
      {     0.5f,  0.5f,  0.5f,   0.0f,1.0f, 0.0f, 1.0f, 0.0f, 0 },
      {    -0.5f,  0.5f,  0.5f,   0.0f,1.0f, 0.0f, 0.0f, 0.0f, 0 },
      {    -0.5f,  0.5f, -0.5f,   0.0f,1.0f, 0.0f, 0.0f, 1.0f, 0 },
    };
    vert_count = COUNT_OF(vertices);
    u32 indices[COUNT_OF(vertices)] = { 0 };
    for(u32 i = 0; i < COUNT_OF(vertices); ++i) indices[i] = i;

    
    Vertex plane_vertices[] = {
      {-plane_length, 0.0f, -plane_length, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 2},
      {plane_length, 0.0f, -plane_length, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 2},
      {plane_length, 0.0f, plane_length, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 2},
      {-plane_length, 0.0f, plane_length, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 2},
    };
    u32 plane_indices[6] = { 0, 1, 2, 2, 3, 0};
    // describe the scene
    materials.push_back({
	.albedo = glm::vec3(1,1,1),
	.emmisive = 1.0f,
	.metallic = 0.0f,
	.roughness = 0.0f,
      }); // white light 0
    materials.push_back({
    .albedo = glm::vec3(1,1,1),
    .emmisive = 0.0f,
    .metallic = 0.0f,
    .roughness = 0.0f,
        }); // matte white 1
    materials.push_back({
	.albedo = glm::vec3(1,0,0),
	.emmisive = 0.0f,
	.metallic = 0.0f,
	.roughness = 0.0f,
      }); // matte red 2
    materials.push_back({
	.albedo = glm::vec3(0,1,0),
	.emmisive = 0.0f,
	.metallic = 0.0f,
	.roughness = 0.0f,
      }); // matte green 3
    materials.push_back({
    .albedo = glm::vec3(0,0,1),
    .emmisive = 0.0f,
    .metallic = 0.0f,
    .roughness = 0.0f,
        }); // matte blue 4

    // vert_id, mat_id
    
    scene.emplace_back(glm::vec3(plane_length, 0, 0), 1, 1); // floor
    scene.emplace_back(glm::vec3(plane_length, plane_length*2, 0), glm::vec3(1, 0, 0), glm::radians(180.0f), 1, 1); // ceiling
    scene.emplace_back(glm::vec3(plane_length, (plane_length*2.0f)-0.1f, 0), glm::vec3(1, 0, 0), glm::radians(180.0f), glm::vec3(0.3), 1, 0); // ceiling light
    scene.emplace_back(glm::vec3(plane_length, plane_length, plane_length), glm::vec3(1,0,0), glm::radians(-90.0f), 1, 3); // right wall
    scene.emplace_back(glm::vec3(plane_length, plane_length, -plane_length), glm::vec3(1, 0, 0), glm::radians(90.0f), 1, 2); // left wall
    scene.emplace_back(glm::vec3(plane_length*2, plane_length, 0), glm::vec3(0, 0, 1), glm::radians(90.0f), 1, 1); // back wall
    scene.emplace_back(glm::vec3(1.5*plane_length, 1.1, -0.5*plane_length), 2, 1); // back left object
    scene.emplace_back(glm::vec3(0.5*plane_length, 1.1, 0.5*plane_length), 2, 1); // front right object

    scene_buff.create(buffer, sizeof(SceneGeometry)*scene.size(), scene.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    mat_buff.create(buffer, sizeof(Material)*materials.size(), materials.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    cube_vbo.create(buffer, sizeof(Vertex)*vert_count, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    cube_ibo.create(buffer, sizeof(u32)*vert_count, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    plane_vbo.create(buffer, sizeof(Vertex)*COUNT_OF(plane_vertices), plane_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    plane_ibo.create(buffer, sizeof(u32)*COUNT_OF(plane_indices), plane_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    createSphere(sphere_vbo, sphere_ibo, buffer, sphere_v_count, sphere_i_count, 1);
    output_image.cmdTransitionLayout(buffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  });

  std::vector<const char*> texture_paths {
    "../../../textures/sample_wood_texture.png",
    "../../../textures/sample_stone_texture.jpeg",
    "../../../textures/solid_white.jpg",
    "../../../textures/sample_obsidian_texture.jpg",
  };
  std::vector<AllocatedImage> block_textures(texture_paths.size());
  bool result = createImageFromFile(texture_paths.data(), block_textures.data(), (u32) block_textures.size(), true);
  if(!result) exit(1);

  Blas scene_blases[3];
  scene_blases[0].add_buffers(cube_vbo, cube_ibo, vert_count, vert_count);
  scene_blases[1].add_buffers(plane_vbo, plane_ibo, 4, 6);
  scene_blases[2].add_buffers(sphere_vbo, sphere_ibo, sphere_v_count, sphere_i_count);
  Blas::build_blas(scene_blases, COUNT_OF(scene_blases));

  Tlas tlas;
  for (SceneGeometry geometry : scene) {
    tlas.add_instance(geometry.vert_id, 0, geometry.transform);
  }
  tlas.build_tlas(scene_blases, (u32) scene.size());
  info_log("Built accleration structures");
  
  Camera camera(glm::vec3(-6, plane_length, 0), glm::vec3(1, 0, 0));
  
  DescSet global_set;
  global_set.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR); // camera
  global_set.add_binding(1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // tlas
  global_set.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR); // output image

  DescSet scene_set;
  scene_set.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, COUNT_OF(scene_blases), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // vertex buffers
  scene_set.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, COUNT_OF(scene_blases), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // index buffers
  scene_set.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // scene metadata
  scene_set.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (u32) scene.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // scene metadata
  scene_set.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // scene metadata
  DescSet::allocate_sets(1, &global_set);
  DescSet::allocate_sets(1, &scene_set);

  std::vector<VkDescriptorBufferInfo> vertex_info;
  std::vector<VkDescriptorBufferInfo> index_info;
  std::vector<VkDescriptorImageInfo> textures_info;
  vertex_info.emplace_back(cube_vbo.get_desc_info());
  index_info.emplace_back(cube_ibo.get_desc_info());

  vertex_info.emplace_back(plane_vbo.get_desc_info());
  index_info.emplace_back(plane_ibo.get_desc_info());

  vertex_info.emplace_back(sphere_vbo.get_desc_info());
  index_info.emplace_back(sphere_ibo.get_desc_info());

  for (u32 i = 0; i < block_textures.size(); ++i) {
    textures_info.emplace_back(block_textures[i].get_desc_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
  }

  WriteDescSet writes[] = {
    global_set.make_write(camera.ubo.get_desc_info(0, sizeof(CameraData)), 0),
    global_set.make_write(tlas.get_desc_info(), 1),
    global_set.make_write(output_image.get_desc_info(VK_IMAGE_LAYOUT_GENERAL), 2),
    
    scene_set.make_write_array(vertex_info.data(), 0),
    scene_set.make_write_array(index_info.data(), 1),
    scene_set.make_write(scene_buff.get_desc_info(), 2),
    //scene_set.make_write_array(textures_info.data(), 3),
    scene_set.make_write(mat_buff.get_desc_info(), 4),
  };
  DescSet::update_writes(writes, COUNT_OF(writes));
  WriteDescSet image_writes[] = {
  scene_set.make_write_array(textures_info.data(), 3),
  };
  DescSet::update_writes(writes, COUNT_OF(writes));
  info_log("Staged resources");
  
  DescSet sets[] = { global_set, scene_set };
  RtProgram rt_program;
  rt_program.init_shader_groups("../../../shaders/raytrace.rgen",
				"../../../shaders/raytrace.rmiss",
				"../../../shaders/raytrace.rchit", sets, COUNT_OF(sets));
  rt_program.create_sbt();

  float dt = 0;
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  glfwSetWindowUserPointer(window, &camera);
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
  camera.data = camera.ubo.map();

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    dt = (float) glfwGetTime() - dt;
    check_input(window, camera, dt);
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    draw_gui(rt_program);
    ImGui::Render();
    VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    begin_info.clearValueCount = 0;
    begin_info.renderPass = vkcontext.swapchain.render_pass;
    begin_info.renderArea.offset = { 0, 0 };
    begin_info.renderArea.extent = { 1920, 1080 };

    auto& frame_data = vkcontext.StartFrame();
    rt_program.bind(frame_data.cmd_buff);
    DescSet sets[] = {global_set.get_copy(), scene_set.get_copy(), };
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
