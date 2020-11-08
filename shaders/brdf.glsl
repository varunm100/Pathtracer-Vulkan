#include "random.glsl"
#include "raycommon.glsl"

#define PI 3.1415926

float ggxNormalDistribution(float NdotH, float roughness){
  float a2 = roughness * roughness;
  float d = ((NdotH * a2 - NdotH) * NdotH + 1);
  return a2 / (d * d * PI);
}

float schlickMaskingTerm(float NdotL, float NdotV, float roughness){
	// Karis notes they use alpha / 2 (or roughness^2 / 2)
	float k = roughness*roughness / 2;

	// Compute G(v) and G(l).  These equations directly from Schlick 1994
	//     (Though note, Schlick's notation is cryptic and confusing.)
	float g_v = NdotV / (NdotV*(1 - k) + k);
	float g_l = NdotL / (NdotL*(1 - k) + k);
	return g_v * g_l;
}

vec3 schlickFresnel(vec3 f0, float lDotH)
{
	return f0 + (vec3(1.0f, 1.0f, 1.0f) - f0) * pow(1.0f - lDotH, 5.0f);
}

// vec3 getGGXMicrofacet(inout uint randSeed, float roughness, float3 hitNorm)
// {
// 	// Get our uniform random numbers
// 	vec2 randVal = float2(nextRand(randSeed), nextRand(randSeed));

// 	// Get an orthonormal basis from the normal
// 	vec2 B = getPerpendicularVector(hitNorm);
// 	float3 T = cross(B, hitNorm);

// 	// GGX NDF sampling
// 	float a2 = roughness * roughness;
// 	float cosThetaH = sqrt(max(0.0f, (1.0-randVal.x)/((a2-1.0)*randVal.x+1) ));
// 	float sinThetaH = sqrt(max(0.0f, 1.0f - cosThetaH * cosThetaH));
// 	float phiH = randVal.y * M_PI * 2.0f;

// 	// Get our GGX NDF sample (i.e., the half vector)
// 	return T * (sinThetaH * cos(phiH)) +
//            B * (sinThetaH * sin(phiH)) +
//            hitNorm * cosThetaH;
// }
