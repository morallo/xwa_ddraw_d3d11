#define GENERIC_POV_SCALE 44.0

// ShadowMapVertexShaderMatrixCB (the same struct is used for both the vertex and pixel shader)
cbuffer ConstantBuffer : register(b5)
{
	matrix Camera;
	matrix lightWorldMatrix;
	// 128 bytes
	
	uint sm_enabled, sm_debug;
	float sm_light_size, sm_blocker_radius;

	float sm_aspect_ratio, sm_bias, sm_max_edge_distance, sm_pcss_radius;

	float3 POV;
	float OBJrange;

	float sm_FOVscale, sm_y_center, sm_metric_mult, sm_z_factor;

	uint sm_PCSS_enabled, sm_pcss_samples;
	float sm_black_level, sm_unused1;
};

