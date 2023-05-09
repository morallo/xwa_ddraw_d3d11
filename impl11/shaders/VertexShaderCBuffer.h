// VertexShaderCBuffer
cbuffer ConstantBuffer : register(b1)
{
	float4 viewportScale;

	float s_V0x08B94CC;
	float s_V0x05B46B4;
	float s_V0x05B46B4_Offset;
	float unused1;

	float4 projectionParametersVS;

	float aspect_ratio;
	uint apply_uv_comp;
	float z_override, sz_override;

	float mult_z_override, bPreventTransform, bFullTransform, scale_override;
};
