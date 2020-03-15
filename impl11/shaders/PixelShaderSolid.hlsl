// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes, 2019

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

/* // I put this here just to debug bracket detection through the bIsBackground flag
// PixelShaderCBuffer
cbuffer ConstantBuffer : register(b0)
{
	float brightness;			// Used to dim some elements to prevent the Bloom effect -- mostly for ReShade compatibility
	uint DynCockpitSlots;		// (Unused here) How many DC slots will be used.
	uint bUseCoverTexture;		// (Unused here) When set, use the first texture as cover texture for the dynamic cockpit
	uint bIsHyperspaceAnim;		// 1 if we're rendering the hyperspace animation
	// 16 bytes

	uint bIsLaser;				// 1 for Laser objects, setting this to 2 will make them brighter (intended for 32-bit mode)
	uint bIsLightTexture;		// 1 if this is a light texture, 2 will make it brighter (intended for 32-bit mode)
	uint bIsEngineGlow;			// 1 if this is an engine glow textures, 2 will make it brighter (intended for 32-bit mode)
	uint bInHyperspace;			// 1 if we're rendering while in hyperspace
	// 32 bytes

	float fBloomStrength;		// General multiplier for the bloom effect
	float fPosNormalAlpha;		// Override for pos3D and normal output alpha
	float fSSAOMaskVal;			// SSAO mask value
	float fSSAOAlphaOfs;			// Additional offset substracted from alpha when rendering SSAO. Helps prevent halos around transparent objects.
	// 48 bytes

	uint bIsShadeless;
	float fGlossiness, fSpecInt, fNMIntensity;
	// 64 bytes

	float fSpecVal, fDisableDiffuse;
	uint AC_debug, bIsBackground;
	// 80 bytes
};
*/

struct PixelShaderInput
{
	float4 pos   : SV_POSITION;
	float4 color : COLOR0;
	float2 tex   : TEXCOORD0;
	float3 pos3D : TEXCOORD1;
};

struct PixelShaderOutput
{
	float4 color		: SV_TARGET0;
	float4 bloom		: SV_TARGET1;
	float4 pos3D		: SV_TARGET2;
	float4 normal	: SV_TARGET3;
	float4 ssaoMask : SV_TARGET4;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color		= input.color;
	output.bloom		= 0;
	output.pos3D		= 0;
	output.normal	= 0;
	output.ssaoMask = 1;
	// DEBUG
	//if (bIsBackground)
	//	output.color = float4(0, 0, 1, 1);
	// DEBUG
	return output;
}
