#pragma once
#include "Context.h"
#include <glm/glm.hpp>
#include "Camera.h"
#include "Blas.h"
#include "Image.h"
#include "Descriptors.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vert {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 uv;

  bool operator==(const Vert& other) const {
    return pos == other.pos && normal == other.normal && uv == other.uv;
  }
};

namespace std {
    template<> struct hash<Vert> {
        size_t operator()(Vert const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.uv) << 1);
        }
    };
}

struct Material {
  glm::vec4 albedo{1,1,1,0}; // w = materialType
  glm::vec3 emission{0,0,0};

  float metallic{0};
  float roughness{0};
  float ior{1.45f};

  glm::vec3 tex_ids{-1,-1,-1}; // albedo, metallic roughness, normal
};

struct GeometryData {
  std::vector<Vert> vertices;
  std::vector<u32> indices;

  void load_obj(const std::string& filename); // only supports wavefront .obj files for now
};

struct Light {
  glm::vec3 pos;
  glm::vec3 emission;
  glm::vec3 u;
  glm::vec3 v;
  glm::vec3 radius_area_type;
};

struct SceneBuffers {
  std::vector<AllocatedBuffer> vbos;
  std::vector<AllocatedBuffer> ibos;
  std::vector<AllocatedImage> textures;
  AllocatedBuffer scene_buffer;
  AllocatedBuffer mat_buffer;
  AllocatedBuffer light_buffer;
};

struct Scene {
  Tlas tlas;
  std::vector<Blas> blases;
  SceneBuffers scene_buffers;
  DescSet scene_set;
  
  std::vector<Light> lights;
  std::vector<SceneGeometry> scene_geometry;
  std::vector<GeometryData> geometries;
  std::vector<std::string> textures;
  std::vector<Material> materials;

  std::unordered_map<std::string, u32> loaded_geometries;
  std::unordered_map<std::string, u32> loaded_textures;
  std::unordered_map<std::string, u32> loaded_materials;
  
  u32 add_mesh(const std::string &filename);
  u32 add_texture(const std::string &filename);
  u32 add_material(const Material& mat, const std::string &filename);

  bool Load_Scene(std::string& filename);
  bool Build_Structures();
  Camera* camera;
};
