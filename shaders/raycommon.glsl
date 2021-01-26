#define PI 3.1415926
#define EPS 0.001
#define INFINITY  1000000.0

struct Material {
  vec4 albedo; // w = materialType
  vec3 emission;

  float metallic;
  float roughness;
  float ior;

  vec3 tex_ids; // albedo, metallic rougness, normal
};

struct Light {
  vec3 pos;
  vec3 emission;
  vec3 u;
  vec3 v;
  vec3 radius_area_type;
};

struct hitPayload {
  vec3 normal;
  vec2 uv;
  uint mat_id;
  float t;
};

vec3 less_than(vec3 f, float value) {
    return vec3(
        (f.x < value) ? 1.0f : 0.0f,
        (f.y < value) ? 1.0f : 0.0f,
        (f.z < value) ? 1.0f : 0.0f);
}

vec3 linear_to_srgb(vec3 rgb){
    rgb = clamp(rgb, 0.0f, 1.0f);
     
    return mix(
        pow(rgb, vec3(1.0f / 2.4f)) * 1.055f - 0.055f,
        rgb * 12.92f,
        less_than(rgb, 0.0031308f)
    );
}
 
vec3 srgb_to_linear(vec3 rgb){
    rgb = clamp(rgb, 0.0f, 1.0f);
     
    return mix(
        pow(((rgb + 0.055f) / 1.055f), vec3(2.4f)),
        rgb / 12.92f,
        less_than(rgb, 0.04045f)
    );
}

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0f, 1.0f);
}
