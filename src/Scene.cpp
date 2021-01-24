#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include "Scene.h"
#include "CmdUtils.h"

void GeometryData::load_obj(const std::string& filename) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  info_log("Loading model, {}", filename);
  bool loaded = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str());

  assert(loaded && "failed to load model");

  if (!warn.empty()) { warn_log(filename + ", " + warn); }
  assert_log(err.empty(), filename + ", " + err);

  std::unordered_map<Vert, u32> unique_vertices{};
  for (size_t s = 0; s < shapes.size(); ++s) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f) {
      size_t fv = 3;
      for (size_t v = 0; v < fv; ++v) {
	tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

	tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
	tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
	tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

	tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
	tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
	tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

	tinyobj::real_t tx = 0;
	tinyobj::real_t ty = 0;
        if (!attrib.texcoords.empty()) {
	  tx = attrib.texcoords[2 * idx.texcoord_index + 0];
	  ty = 1-attrib.texcoords[2 * idx.texcoord_index + 1]; // flip y axis
        }

        Vert vert{};
        vert.pos.x = vx;
	vert.pos.y = vy;
	vert.pos.z = vz;

	vert.normal.x = nx;
	vert.normal.y = ny;
	vert.normal.z = nz;

        vert.uv.x = tx;
	vert.uv.y = ty;

        if (unique_vertices.count(vert) == 0) {
          unique_vertices[vert] = static_cast<u32>(vertices.size());
          vertices.push_back(vert);
        }
	indices.push_back(unique_vertices[vert]);
      }
      index_offset += fv;
    }
  }
}

u32 Scene::add_mesh(const std::string &filename) {
  if(loaded_geometries.find(filename) != loaded_geometries.end()) return loaded_geometries[filename];

  geometries.emplace_back();
  geometries.back().load_obj(filename);
  loaded_geometries[filename] = (u32) geometries.size() - 1;

  return (u32) geometries.size() - 1;
}

u32 Scene::add_texture(const std::string &filename) {
  if(loaded_textures.find(filename) != loaded_textures.end()) return loaded_textures[filename];

  textures.emplace_back(filename);
  loaded_textures[filename] = (u32) textures.size() - 1;
  return (u32) textures.size() - 1;
}

u32 Scene::add_material(const Material& mat, const std::string &filename) {
  if(loaded_materials.find(filename) != loaded_materials.end()) return loaded_materials[filename];

  materials.push_back(mat);
  loaded_materials[filename] = (u32) materials.size() - 1;
  return (u32) materials.size() - 1;
}

bool Scene::Load_Scene(std::string &filename) {
  const u32 max_length = 2048;
  FILE* file = fopen(filename.c_str(), "r");

  if (!file) {
    err_log("Could not load scene file, {}", filename);
    return false;
  }
  info_log("Loading Scene... {}", filename);

  std::string path = filename.substr(0, filename.find_last_of("/\\")) + "/";
  char line[max_length];

  bool camera_added= false;
  while (fgets(line, max_length, file)) {
    if(line[0] == '#' || line[0] == '\n') continue;

    char name[max_length] = {0};

    // material
    if (sscanf(line, " material %s", name) == 1) {
      Material mat;
      char albedo_tex[100] = "None";
      char metallic_roughness_tex[100] = "None";
      char normal_tex[100] = "None";

      while (fgets(line, max_length, file)) {
        if (strchr(line, '}')) break;

        sscanf(line, " name %s", name);
        sscanf(line, " color %f %f %f", &mat.albedo.x, &mat.albedo.y,
	       &mat.albedo.z);
        sscanf(line, " emission %f %f %f", &mat.emission.x, &mat.emission.y,
	       &mat.emission.z);
        sscanf(line, " materialType %f", &mat.albedo.w);
        sscanf(line, " metallic %f", &mat.metallic);
        sscanf(line, " roughness %f", &mat.roughness);
        sscanf(line, " ior %f", &mat.ior);
        // scanf_s(line, " transmittance %f", &mat.transmittance);
        
        sscanf(line, " albedoTexture %s", albedo_tex);
        sscanf(line, " metallicRoughness %s", metallic_roughness_tex);
        sscanf(line, " normalTexture %s", normal_tex);
      }

      if (strcmp(albedo_tex, "None") != 0)
        mat.tex_ids.x = (float) add_texture(path + albedo_tex);

      if (strcmp(metallic_roughness_tex, "None") != 0)
        mat.tex_ids.y = (float) add_texture(path + metallic_roughness_tex);

      if (strcmp(normal_tex, "None") != 0)
        mat.tex_ids.z = (float) add_texture(path + normal_tex);

      add_material(mat, name);
    }

    // mesh
    if (strstr(line, "mesh")) {
      std::string filename;
      glm::vec3 pos{0,0,0};
      glm::vec3 scale{1,1,1};

      u32 mat_id = 0;
      char mesh_name[200]{"None"};

      while (fgets(line, max_length, file)) {
        if(strchr(line, '}')) break;

	char file[2048];
	char mat_name[100];

	sscanf(line, " name %[^\t\n]s", mesh_name);

        if (sscanf(line, " file %s", file) == 1) {
	  filename = path + file;
        }

        if (sscanf(line, " material %s", mat_name) == 1) {
          if (loaded_materials.find(mat_name) != loaded_materials.end()) {
	    mat_id = loaded_materials[mat_name];
          } else {
            warn_log("Could not find material {}", mat_name);
          }
        }

	sscanf(line, " position %f %f %f", &pos.x, &pos.y, &pos.z);
	sscanf(line, " scale %f %f %f", &scale.x, &scale.y, &scale.z);
	// TODO: take rotation here
      }

      if (!filename.empty()) {
	u32 mesh_id = add_mesh(filename);
	glm::vec3 axis{1,1,1};
	scene_geometry.emplace_back(pos, axis, 0, scale, mesh_id, mat_id);
      }
    }

    // light
    if (strstr(line, "light")) {
      Light light;
      char light_type[20] = "None";
      glm::vec3 v1;
      glm::vec3 v2;

      while (fgets(line, max_length, file)) {
        if (strchr(line, '}')) break;

	sscanf(line, " position %f %f %f", &light.pos.x, &light.pos.y, &light.pos.z);
	sscanf(line, " emission %f %f %f", &light.emission.x, &light.emission.y, &light.emission.z);

	sscanf(line, " radius %f", &light.radius_area_type.x);
	sscanf(line, " v1 %f %f %f", &v1.x, &v1.y, &v1.z);
	sscanf(line, " v2 %f %f %f", &v2.x, &v2.y, &v2.z);
	sscanf(line, " type %s", &light_type);
      }

      if (strcmp(light_type, "Quad") == 0) {
	light.radius_area_type.z  = 0; // type 0 = quad
	light.u = v1 - light.pos;
	light.v = v2 - light.pos;
	light.radius_area_type.y = glm::length(glm::cross(light.u, light.v));
      } else if (strcmp(light_type, "Sphere") == 0) {
	light.radius_area_type.z = 1; // type 1 = sphere
	light.radius_area_type.y = 4.0f * 3.14159265f * light.radius_area_type.x;
      } else {
	warn_log("Unknown light type, {}", light_type);
      }
      lights.push_back(light);
    }

    if (strstr(line, "Camera")) {
      glm::vec3 pos;
      glm::vec3 look_at;
      float fov;
      float aperture = 0, focal_dist = 1;

      while (fgets(line, max_length, file)) {
        if(strchr(line, '}')) break;

	sscanf(line, " position %f %f %f", &pos.x, &pos.y, &pos.z);
	sscanf(line, " lookAt %f %f %f", &look_at.x, &look_at.y, &look_at.z);
	sscanf(line, " aperture %f ", &aperture);
	sscanf(line, " focaldist %f", &focal_dist);
	sscanf(line, " fov %f", &fov);
      }

      camera = new Camera(pos, look_at, fov);
      camera_added = true;
    }
    if (!camera_added) {
      camera = new Camera(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, -10.0f), 45.0f);
    }
    // TODO: add camera and renderer settings
  }
  return true;
}

bool Scene::Build_Structures() {
  vkutil::immediate_submit([&](VkCommandBuffer buffer) {
    // stage scene desc. data
    size_t desc_size = scene_geometry.size()*sizeof(SceneGeometry);
    scene_buffers.scene_buffer.create(buffer, desc_size, scene_geometry.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // stage materials data
    size_t mats_size = materials.size()*sizeof(Material);
    scene_buffers.mat_buffer.create(buffer, mats_size, materials.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // stage lights data
    size_t lights_size = lights.size()*sizeof(Light);
    scene_buffers.light_buffer.create(buffer, lights_size, lights.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // stage vertex and index buffers
    scene_buffers.vbos.resize(geometries.size());
    scene_buffers.ibos.resize(geometries.size());
    for (size_t g = 0; g < geometries.size(); ++g) {
      scene_buffers.vbos[g].create(buffer, (geometries[g].vertices.size() * sizeof(Vert)), geometries[g].vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
      scene_buffers.ibos[g].create(buffer, (geometries[g].indices.size() * sizeof(u32)), geometries[g].indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

    // stage images
    scene_buffers.textures.resize(textures.size());
    for (u32 t = 0; t < textures.size(); ++t) {
      const char* filename = textures[t].c_str();
      AllocatedImage& texture = scene_buffers.textures[t];
    
      int width, height, channels;
      stbi_uc* pixels = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
      stbi_set_flip_vertically_on_load(false);
      if (!pixels) {
	err_log("Failed to load image: {}", filename);
      } else {
        info_log("Loading texture, {}", filename);
        texture.create(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, { (u32)width, (u32)height, 1});
	texture.cmdTransitionLayout(buffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkutil::toImage(buffer, texture.image, 0, width * height * 4, pixels, {(u32)width, (u32)height, 1});
	texture.cmdTransitionLayout(buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      }
      stbi_image_free(pixels);
    }
  });

  // build scene blases
  blases.resize(geometries.size());
  for (u32 b = 0; b < geometries.size(); ++b) {
    blases[b].add_buffers(scene_buffers.vbos[b], scene_buffers.ibos[b], (u32) geometries[b].vertices.size(), (u32) geometries[b].indices.size());
  }
  Blas::build_blas(blases.data(), (u32) blases.size());
  
  // build scene tlas
  for (auto& geometry : scene_geometry) {
    tlas.add_instance(geometry.vert_id, 0, geometry.transform);
  }
  tlas.build_tlas(blases.data(), (u32) scene_geometry.size());

  // setup desc sets
  scene_set.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (u32) geometries.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  scene_set.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (u32) geometries.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  scene_set.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // scene metadata
  scene_set.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (u32) textures.size(), VK_SHADER_STAGE_RAYGEN_BIT_KHR); // texture
  scene_set.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR); // materials
  scene_set.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR); // lights

  DescSet::allocate_sets(1, &scene_set);

  std::vector<VkDescriptorBufferInfo> vertices_info(geometries.size());
  std::vector<VkDescriptorBufferInfo> indices_info(geometries.size());
  std::vector<VkDescriptorImageInfo> textures_info(textures.size());

  AllocatedBuffer::fill_desc_infos(scene_buffers.vbos.data(), vertices_info.data(), (u32) geometries.size());
  AllocatedBuffer::fill_desc_infos(scene_buffers.ibos.data(), indices_info.data(), (u32) geometries.size());
  AllocatedImage::fill_desc_infos(scene_buffers.textures.data(), textures_info.data(), (u32) textures.size(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (textures.size() > 0) {
    WriteDescSet writes[] = {
      scene_set.make_write_array(vertices_info.data(), 0),
      scene_set.make_write_array(indices_info.data(), 1),
      scene_set.make_write(scene_buffers.scene_buffer.get_desc_info(), 2),
      scene_set.make_write_array(textures_info.data(), 3),
      scene_set.make_write(scene_buffers.mat_buffer.get_desc_info(), 4),
      scene_set.make_write(scene_buffers.light_buffer.get_desc_info(), 5),
    };
    DescSet::update_writes(writes, COUNT_OF(writes));
  } else {
    WriteDescSet writes[] = {
      scene_set.make_write_array(vertices_info.data(), 0),
      scene_set.make_write_array(indices_info.data(), 1),
      scene_set.make_write(scene_buffers.scene_buffer.get_desc_info(), 2),
      //      scene_set.make_write_array(textures_info.data(), 3),
      scene_set.make_write(scene_buffers.mat_buffer.get_desc_info(), 4),
      scene_set.make_write(scene_buffers.light_buffer.get_desc_info(), 5),
    };
    DescSet::update_writes(writes, COUNT_OF(writes));
  }
  return true;
}
