#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "scatter.glsl"

struct Vertex {
  vec3 pos;
  vec3 normal;
  vec2 uv;
  uint tex_id;
};

struct SceneGeometry {
  mat4 transform;
  mat4 transformIT;
  uint vert_id;
  uint mat_id;
};

struct Material {
  vec3 albedo;
  vec3 emmisive;
  vec3 specular_color;
  float percent_specular;
  float roughness;
};

layout(binding = 1, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 0, set = 1, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout(binding = 1, set = 1) buffer Indices { uint i[]; } indices[];
layout(binding = 2, set = 1, scalar) buffer Scene { SceneGeometry g[]; } scene;
layout(binding = 3, set = 1) uniform sampler2D textures[];
layout(binding = 4, set = 1, scalar) buffer Materials { Material m[]; } materials;
layout(location = 0) rayPayloadInEXT hitPayload prd;
hitAttributeEXT vec3 attribs;

void main() {
  uint vert_id = scene.g[gl_InstanceID].vert_id;
  uint mat_id = scene.g[gl_InstanceID].mat_id;
  ivec3 ind = ivec3(indices[nonuniformEXT(vert_id)].i[3 * gl_PrimitiveID + 0],
                    indices[nonuniformEXT(vert_id)].i[3 * gl_PrimitiveID + 1],
                    indices[nonuniformEXT(vert_id)].i[3 * gl_PrimitiveID + 2]);

  Vertex v0 = vertices[nonuniformEXT(vert_id)].v[ind.x];
  Vertex v1 = vertices[nonuniformEXT(vert_id)].v[ind.y];
  Vertex v2 = vertices[nonuniformEXT(vert_id)].v[ind.z];

  const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  vec3 normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
  normal = normalize(vec3(scene.g[gl_InstanceID].transformIT * vec4(normal, 0.0)));

  vec3 hit_pos = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
  hit_pos = vec3(scene.g[gl_InstanceID].transform * vec4(hit_pos, 1.0));

  uint tex_index = nonuniformEXT(v0.tex_id);
  vec2 tex_coord = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;

  vec4 tex_color = texture(textures[tex_index], tex_coord);
  vec3 diffuse = materials.m[mat_id].albedo;
  prd = scatter(0, diffuse, gl_WorldRayDirectionEXT, normal, gl_HitTEXT, prd.seed);
}