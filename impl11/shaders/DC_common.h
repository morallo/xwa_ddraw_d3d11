// DCPixelShaderCBuffer
cbuffer ConstantBuffer : register(b1)
{
	float4 src[MAX_DC_COORDS_PER_TEXTURE];		  // HLSL packs each element in an array in its own 4-vector (16 bytes) slot, so .xy is src0 and .zw is src1
	float4 dst[MAX_DC_COORDS_PER_TEXTURE];
	uint4 bgColor[MAX_DC_COORDS_PER_TEXTURE / 4]; // Background colors to use for the dynamic cockpit, this divide by 4 is because HLSL packs each elem in a 4-vector,
												  // So each elem here is actually 4 bgColors.

	float ct_brightness;	// Cover texture brightness. In 32-bit mode the cover textures have to be dimmed.
	float dc_brightness;	// DC element brightness
	uint noisy_holo;		// If set to 1, the hologram shader will be noisy!
	uint transparent;		// If set to 1, the background will be transparent

	uint use_damage_texture; // If set to 1, force the use of the damage texture
	uint dc_unused0, dc_unused1, dc_unused2;
};

