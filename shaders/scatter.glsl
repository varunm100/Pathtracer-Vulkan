#include "random.glsl"
#include "raycommon.glsl"

hitPayload scatter_lambertian(const vec3 diffuse, const vec3 dir, const vec3 normal, const float t, inout uint seed) {
  const bool is_scattered = dot(dir, normal) < 0;
  const vec4 color_dist = vec4(diffuse, t);
  const vec4 scatter = vec4(normal + RandomInUnitSphere(seed), is_scattered ? 1 : 0);
  return hitPayload(color_dist, scatter, seed);
}

hitPayload scatter_diffuse_light(const vec3 diffuse, const float t, inout uint seed){
  const vec4 color_dist = vec4(diffuse, t);
  const vec4 scatter = vec4(1, 1, 1, 0);
  return hitPayload(color_dist, scatter, seed);
}

hitPayload scatter(uint type, const vec3 diffuse, const vec3 dir, const vec3 normal, const float t, inout uint seed) {
  if(type == 0){
    return scatter_lambertian(diffuse, dir, normal, t, seed);
  } else if(type == 1) {
    return scatter_diffuse_light(diffuse, t, seed);
  }
}
