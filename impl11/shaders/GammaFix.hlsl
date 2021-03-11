//#include "ShaderToyDefs.h"
//#include "ShadertoyCBuffer.h"

// The texture to apply AA to
Texture2D    colorTex  : register(t0);
SamplerState colorSamp : register(s0);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color = 0;

	output.color = colorTex.Sample(colorSamp, input.uv);
	output.color = output.color * output.color; // Gamma correction (approx pow 2.2)

	return output;
}
