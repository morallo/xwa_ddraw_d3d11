#include "material_defs.h"

// PSShadingSystemCB
cbuffer ConstantBuffer : register(b4)
{
	float4 LightVector[2];
	// 16 bytes
	// 32 bytes
	float4 LightColor[2];
	// 48 bytes
	// 64 bytes
	float global_spec_intensity, global_glossiness, global_spec_bloom_intensity, global_bloom_glossiness_mult;
	// 80 bytes
	float saturation_boost, lightness_boost, ssdo_enabled;
	uint ss_debug;
	// 96 bytes
	float sso_disable, ssunused0, ssunused1, ssunused2;
};
