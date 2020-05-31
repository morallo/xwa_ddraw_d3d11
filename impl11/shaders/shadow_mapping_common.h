// ShadowMapVertexShaderMatrixCB (the same struct is used for both the vertex and pixel shader)
cbuffer ConstantBuffer : register(b5)
{
	matrix Camera;
	matrix lightWorldMatrix[MAX_XWA_LIGHTS];
	
	uint sm_enabled, sm_debug;
	float sm_light_size, sm_blocker_radius;

	float sm_aspect_ratio, sm_bias, sm_max_edge_distance, sm_pcss_radius;

	float3 POV;
	float sm_resolution;

	int light_index;
	float sm_FOVscale, sm_y_center, sm_z_factor;

	uint sm_PCSS_enabled, sm_pcss_samples;
	float sm_black_level;
	uint sm_hardware_pcf;

	float4 sm_black_levels[MAX_XWA_LIGHTS / 4];
	float4 OBJrange[MAX_XWA_LIGHTS / 4];
	float4 OBJminZ[MAX_XWA_LIGHTS / 4];
};

inline float get_OBJrange(uint idx) {
	return OBJrange[idx >> 2][idx & 0x03];
}

inline float get_OBJminZ(uint idx) {
	return OBJminZ[idx >> 2][idx & 0x03];
}

inline float MetricZToDepth(uint idx, float Z) {
	const float range = get_OBJrange(idx);
	const float minZ = get_OBJminZ(idx);
	// Objects that are too far away correspond to a depth of 1.0
	if (Z > range + minZ) return 1.0;
	return 0.98 * (Z - minZ) / range;
}

inline float DepthToMetricZ(uint idx, float z) {
	const float range = get_OBJrange(idx);
	const float minZ = get_OBJminZ(idx);
	// This is the inverse of the above: map 0..0.98 to the range OBJminZ to OBJminZ + OBJrange
	// Z values above 0.98 are "unpopulated" during the creation of the shadow map, so they are "at infinity"
	if (z > 0.98) return INFINITY_Z0;
	return z / 0.98 * range + minZ;
}


