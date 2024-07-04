#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"

#ifdef INSTANCED_RENDERING
Texture2DArray texPos  : register(t4);
SamplerState   sampPos : register(s4);
#else
// The position/depth buffer
Texture2D    texPos  : register(t4);
SamplerState sampPos : register(s4);
#endif

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 uv     : TEXCOORD0;
	float3 pos3D  : TEXCOORD1;
#ifdef INSTANCED_RENDERING
	uint   viewId : SV_RenderTargetArrayIndex;
#endif
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;

	// From the vertex shader, the input xy coords are normalized [-1..1] vertex
	// screen coords. So if we apply the proper transforms, we can convert them
	// into post-proc UV coords:
	const float2 uv = (input.pos3D.xy * float2(0.5, -0.5)) + 0.5;
#ifndef INSTANCED_RENDERING
	// In the non-VR path, we need to reject anything outside the viewport:
	if (any(uv < p0) || any(uv > p1))
		discard;
#endif

	// The vertex shader passes the original metric Z coord, so here we can use it
	// to do a depth test and discard pixels that should not be rendered:
	const float  curZ  = input.pos3D.z;
#ifdef INSTANCED_RENDERING
	const float3 depth = texPos.Sample(sampPos, float3(uv, input.viewId)).xyz;
#else
	const float3 depth = texPos.Sample(sampPos, uv).xyz;
#endif

	if (curZ >= 60.0 ||  // Avoid adding speed trails inside the cockpit
	    curZ >= depth.z) // Apply the depth mask
		discard;

	// The speed is encoded in the blue component of the input color, so that's why
	// we multiply by input.color.b.
	output.color.rgb = input.color.b * smoothstep(0.9, 0.6, length(input.uv));
	// The blending is controlled by InitBlendState() when this shader is called. It's
	// equivalent to the direct addition of the speed trail and the existing offscreenBuffer
	// contents.
	output.color.a   = 1.0;

	return output;
}