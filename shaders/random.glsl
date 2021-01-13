#extension GL_EXT_control_flow_attributes : require

// https://github.com/nvpro-samples/optix_prime_baking/blob/332a886f1ac46c0b3eea9e89a59593470c755a0e/random.h
// https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
uint init_random_seed(uint val0, uint val1){
  uint v0 = val0, v1 = val1, s0 = 0;

  [[unroll]] 
  for (uint n = 0; n < 16; n++){
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

uint random_int(inout uint seed){
// LCG values from Numerical Recipes
  return (seed = 1664525 * seed + 1013904223);
}

float rand(inout uint seed){
  return (float(random_int(seed) & 0x00FFFFFF) / float(0x01000000));
}

uint wang_hash(inout uint seed){
  seed = (seed ^ 61) ^ (seed >> 16);
  seed *= 9;
  seed = seed ^ (seed >> 4);
  seed *= 0x27d4eb2d;
  seed = seed ^ (seed >> 15);
  return seed;
}

vec3 cosine_sample_hemisphere(float u1, float u2) {
  vec3 dir;
  float r = sqrt(u1);
  float phi = 2.0 * 3.141592 * u2;
  dir.x = r * cos(phi);
  dir.y = r * sin(phi);
  dir.z = sqrt(max(0.0, 1.0 - dir.x*dir.x - dir.y*dir.y));

  return dir;
}

vec3 uniform_sample_sphere(inout uint rng_state) {
  float r1 = rand(rng_state);
  float r2 = rand(rng_state);
  float z = 1.0 - 2.0 * r1;
  float r = sqrt(max(0.f, 1.0 - z * z));
  float phi = 2.0 * 3.141592 * r2;
  float x = r * cos(phi);
  float y = r * sin(phi);

  return vec3(x, y, z);
}
