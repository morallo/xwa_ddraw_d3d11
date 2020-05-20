#include "shader_common.h"

// ShadertoyCBuffer
cbuffer ConstantBuffer : register(b7)
{
	float iTime, twirl, bloom_strength, srand;
	// 16 bytes
	float2 iResolution;
	uint VRmode;
	float y_center;
	// 32 bytes
	float2 p0, p1; // Limits in uv-coords of the viewport
	// 48 bytes
	matrix viewMat;
	// 112 bytes
	uint bDisneyStyle, hyperspace_phase;
	float tunnel_speed, FOVscale;
	// 128 bytes
	int SunFlareCount;
	float flare_intensity, st_unused0, st_unused1;
	// 144 bytes
	//float SunX, SunY, SunZ, flare_intensity;
	float4 SunCoords[MAX_SUN_FLARES];
	// 208 bytes
	float4 SunColor[MAX_SUN_FLARES];
	// 272 bytes
};
