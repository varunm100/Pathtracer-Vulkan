#include "brdf.glsl"

hitPayload scatter_lambertian(Material mat, const vec3 dir, const vec3 normal, const float t, inout uint seed) {
   bool is_scattered = dot(dir, normal) < 0;
  is_scattered = true;
  const vec4 color_dist = vec4(mat.albedo, t);
  const vec4 scatter = vec4(normal + RandomInUnitSphere(seed), is_scattered ? 1 : 0);
  const vec3 emmisive = mat.emmisive*vec3(1, 1, 1);
  return hitPayload(color_dist, scatter, emmisive, seed);
}

hitPayload scatter(Material mat, const vec3 dir, const vec3 normal, const float t, inout uint seed) {
  return scatter_lambertian(mat, dir, normal, t, seed);
}
