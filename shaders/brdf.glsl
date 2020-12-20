#extension GL_GOOGLE_include_directive : enable

#include "random.glsl"

float SchlickFresnel(float u) {
    float m = clamp(1.0 - u, 0.0, 1.0);
    float m2 = m * m;
    return m2 * m2*m; // pow(m,5)
}

float GTR2(float NDotH, float a) {
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0)*NDotH*NDotH;
    return a2 / (PI * t*t);
}

float SmithG_GGX(float NDotv, float alphaG) {
    float a = alphaG * alphaG;
    float b = NDotv * NDotv;
    return 1.0 / (NDotv + sqrt(a + b - a * b));
}

vec3 mat_eval(vec3 incident, vec3 bsdf_dir, vec3 normal, in Material mat) {
  vec3 N = normal.xyz;
  vec3 V = incident;
  vec3 L = bsdf_dir;

  float NDotL = dot(N, L);
  float NDotV = dot(N, V);

  if(NDotL <= 0.0 || NDotV <= 0.0) return vec3(0.0);

  vec3 H = normalize(L + V);
  float NDotH = dot(N, H);
  float LDotH = dot(L, H);

  float specular = 0.5;
  vec3 specularColor = mix(vec3(1.0) * 0.08 * specular, mat.albedo.xyz, mat.metallic);
  float a = max(0.001, mat.roughness);
  float Ds = GTR2(NDotH, a);
  float FH = SchlickFresnel(LDotH);
  vec3 Fs = mix(specularColor, vec3(1.0), FH);
  float roughg = mat.roughness*0.5 + 0.5;
  roughg = roughg*roughg;
  float Gs = SmithG_GGX(NDotL, roughg) * SmithG_GGX(NDotV, roughg);

  return (mat.albedo.xyz / 3.141592) * (1.0 - mat.metallic) + Gs * Fs * Ds;
}

vec3 mat_sample(vec3 incident, vec3 normal, inout uint rng_state, in Material mat) {
  vec3 N = normal;
  vec3 V = incident;

  vec3 dir;

  float probability = rand(rng_state);
  float diffuseRatio = 0.5 * (1.0 - mat.metallic);

  float r1 = rand(rng_state);
  float r2 = rand(rng_state);

  vec3 UpVector = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
  vec3 TangentX = normalize(cross(UpVector, N));
  vec3 TangentY = cross(N, TangentX);

  if(probability < diffuseRatio) { // do diffuse
    dir = cosine_sample_hemisphere(rng_state);
    dir = TangentX * dir.x + TangentY * dir.y + N * dir.z;
  } else {
    float a = max(0.001, mat.roughness);
    float phi = r1 * 2.0 * 3.141592;

    float cosTheta = sqrt((1.0 - r2) / (1.0 + (a*a - 1.0) *r2));
    float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

    vec3 halfVec = vec3(sinTheta*cosPhi, sinTheta*sinPhi, cosTheta);
    halfVec = TangentX * halfVec.x + TangentY * halfVec.y + N * halfVec.z;

    dir = 2.0*dot(V, halfVec)*halfVec - V;
  }
  return dir;
}

float mat_pdf(vec3 incident, vec3 normal, vec3 bsdf_dir, Material mat) {
  vec3 n = normal;
  vec3 V = incident;
  vec3 L = bsdf_dir;

  float specularAlpha = max(0.001, mat.roughness);

  float diffuseRatio = 0.5 * (1.0 - mat.metallic);
  float specularRatio = 1 - diffuseRatio;

  vec3 halfVec = normalize(L + V);

  float cosTheta = abs(dot(halfVec, n));
  float pdfGTR2 = GTR2(cosTheta, specularAlpha) * cosTheta;

  float pdfSpec = pdfGTR2 / (4.0 * abs(dot(L, halfVec)));
  float pdfDiff = abs(dot(L, n)) * (1.0 / 3.141592);

  return diffuseRatio * pdfDiff + specularRatio * pdfSpec;
}