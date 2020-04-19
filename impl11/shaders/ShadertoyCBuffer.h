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
	float3 SunCoords; // Coordinates in desktop resolution
	float flare_intensity;
	//float2 LightPos; // Coordinates of the associated light
	// 144 bytes
	float4 SunColor;
	// 160 bytes
};
