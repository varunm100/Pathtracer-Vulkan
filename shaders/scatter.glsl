#include "brdf.glsl"

float FresnelReflectAmount(float n1, float n2, vec3 normal, vec3 incident, float f0, float f90){
  // Schlick aproximation
  float r0 = (n1-n2) / (n1+n2);
  r0 *= r0;
  float cosX = -dot(normal, incident);
  if (n1 > n2) {
    float n = n1/n2;
    float sinT2 = n*n*(1.0-cosX*cosX);
    // total internal reflection
    if (sinT2 > 1.0) return f90;
    cosX = sqrt(1.0-sinT2);
  }
  float x = 1.0-cosX;
  float ret = r0+(1.0-r0)*x*x*x*x*x;
  return mix(f0, f90, ret);
}

hitPayload scatter(Material mat, const vec3 dir, const vec3 normal, const float t, inout uint seed) {
  bool is_scattered = dot(dir, normal) < 0;
  const vec4 color_dist = vec4(mat.albedo, t);
  float specular_chance = mat.metallic;
  if(specular_chance > 0.0) {
    specular_chance = FresnelReflectAmount(1.0, mat.ior, dir, normal, mat.metallic, 1.0f);
  }
  const float do_specular = (RandomFloat(seed) < specular_chance) ? 1.0f : 0.0f;
  float ray_probability = (do_specular == 1.0f) ? specular_chance : 1.0f - specular_chance;
  ray_probability = max(ray_probability, 0.001f);
  const vec3 diffuse = normalize(normal + RandomInUnitSphere(seed));
  vec3 specular = normalize(reflect(dir, normal));
  specular = normalize(mix(specular, diffuse, mat.roughness*mat.roughness));
  const vec4 scatter = vec4(mix(diffuse, specular, do_specular), is_scattered ? ray_probability : 0);
  return hitPayload(mat, color_dist, scatter, seed);
}
