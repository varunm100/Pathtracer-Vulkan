#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "brdf.glsl"

float rect_intersect(in vec3 pos, in vec3 u, in vec3 v, in vec4 plane, in vec3 origin, in vec3 dir) {
  vec3 n = vec3(plane);
  float dt = dot(dir, n);
  float t = (plane.w - dot(n, origin)) / dt;
  if(t > EPS) {
    vec3 p = origin + dir*t;
    vec3 vi = p - pos;
    float a1 = dot(u, vi);
    if(a1 >= 0. && a1 <= 1.) {
      float a2 = dot(v, vi);
      if (a2 >= 0. && a2 <= 1.) return t;
    }
  }
  return INFINITY;
}

float power_heuristic(float a, float b) {
  return (a*a)/(a*a+b*b);
}

vec3 sample_quad_light(uint light_id, inout uint rng_state, out vec3 sample_normal) {
  Light light = lights.l[light_id];

  float r1 = rand(rng_state);
  float r2 = rand(rng_state);

  sample_normal = normalize(cross(light.u, light.v));
  vec3 point = light.pos + light.u * r1 + light.v * r2;
  return point;
}

vec3 sample_sphere_light(uint light_id, inout uint rng_state, out vec3 sample_normal) {
  Light light = lights.l[light_id];

  vec3 point = light.pos + uniform_sample_sphere(rng_state) * light.radius_area_type.x;
  sample_normal = normalize(point - light.pos);  
  return point;
}

vec3 sample_light(uint light_id, inout uint rng_state, out vec3 sample_normal) {
  Light light = lights.l[light_id];
  if(light.radius_area_type.z == 0) {
    return sample_quad_light(light_id, rng_state, sample_normal);
  } else if(light.radius_area_type.z == 1) {
    return sample_sphere_light(light_id, rng_state, sample_normal);
  }
}
