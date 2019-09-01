// Copyright (c) 2019 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called to render the HUD FG/BG

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

// If bRenderHUD is set:
// texture0 == HUD foreground
// texture1 == HUD background

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
	float2 tex : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
	float4 bloom : SV_TARGET1;
};

cbuffer ConstantBuffer : register(b0)
{
	float brightness;		// Used to dim some elements to prevent the Bloom effect -- mostly for ReShade compatibility
	uint DynCockpitSlots;	// How many DC slots will be used. This setting was "bShadeless" previously
	uint bUseCoverTexture;	// When set, use the first texture as cover texture for the dynamic cockpit
	uint unused;				// (Used to be bRenderHUD) When set, first texture is HUD foreground and second texture is HUD background
	// 16 bytes

	uint bIsLaser;					// 1 for Laser objects, setting this to 2 will make them brighter (intended for 32-bit mode)
	uint bIsLightTexture;			// 1 if this is a light texture, 2 will make it brighter (intended for 32-bit mode)
	uint bIsEngineGlow;				// 1 if this is an engine glow textures, 2 will make it brighter (intended for 32-bit mode)
	// unused
};

#define MAX_DC_COORDS 8
cbuffer ConstantBuffer : register(b1)
{
	float4 src[MAX_DC_COORDS];		  // HLSL packs each element in an array in its own 4-vector (16 bytes) slot, so .xy is src0 and .zw is src1
	float4 dst[MAX_DC_COORDS];
	uint4 bgColor[MAX_DC_COORDS / 4]; // Background colors to use for the dynamic cockpit, this divide by 4 is because HLSL packs each elem in a 4-vector,
									  // So each elem here is actually 4 bgColors.

	float ct_brightness;				  // Cover texture brightness. In 32-bit mode the cover textures have to be dimmed.
	// unused1, unused2, unused3
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float4 texelColorBG = texture1.Sample(sampler1, input.tex);
	float alpha = texelColor.w;
	float alphaBG = texelColorBG.w;
	float3 diffuse = input.color.xyz;
	uint i;

	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);
	output.color = texelColor;

	// This code assumes bRenderHUD is set -- so this is flag is now
	// redundant

	// Render the captured HUD, execute the move_region commands.
	// texture0 == HUD foreground
	// texture1 == HUD background
	
	// When DC is off (DynCockpitSlots == 0) we can return early by
	// simply combining texelColor with texelColorBG
	if (DynCockpitSlots == 0) {
		// Do the alpha blending
		texelColor.xyz = lerp(texelColorBG.xyz, texelColor.xyz, alpha);
		texelColor.w += 3.25 * alphaBG;
		output.color = texelColor;
		return output;
	}

	// Execute the move_region commands: erase source regions
	[unroll]
	for (i = 0; i < DynCockpitSlots; i++)
		if (input.tex.x >= src[i].x && input.tex.x <= src[i].z &&
			input.tex.y >= src[i].y && input.tex.y <= src[i].w) {
			texelColor.w = 0;
			alpha = 0;
			alphaBG = 0;
		}

	// Execute the move_region commands: copy regions
	[unroll]
	for (i = 0; i < DynCockpitSlots; i++) {
		float2 delta = dst[i].zw - dst[i].xy;
		float2 s = (input.tex - dst[i].xy) / delta;
		float2 dyn_uv = lerp(src[i].xy, src[i].zw, s);
		if (dyn_uv.x >= src[i].x && dyn_uv.x <= src[i].z &&
			dyn_uv.y >= src[i].y && dyn_uv.y <= src[i].w)
		{
			// Sample the HUD FG and BG from a different location:
			texelColor = texture0.Sample(sampler0, dyn_uv);
			alpha = texelColor.w;
			texelColorBG = texture1.Sample(sampler1, dyn_uv);
			alphaBG = texelColorBG.w;
		}
	}

	// Do the alpha blending
	texelColor.xyz = lerp(texelColorBG.xyz, texelColor.xyz, alpha);
	texelColor.w += 3.25 * alphaBG;
	output.color = texelColor;
	return output;
}
