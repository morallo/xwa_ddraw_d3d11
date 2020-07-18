// VertexShaderCBuffer
cbuffer ConstantBuffer : register(b0)
{
	float4 vpScale;

	float aspect_ratio;
	uint apply_uv_comp;
	float z_override, sz_override;

	float mult_z_override, bPreventTransform, bFullTransform, scale_override;
};
