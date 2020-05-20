/*
 * From: https://takinginitiative.wordpress.com/2011/05/15/directx10-tutorial-10-shadow-mapping/
 * Empty PixelShader needed to populate the depth stencil used
 * for shadow mapping
 */
struct SHADOW_PS_INPUT
{
	float4 pos : SV_POSITION;
};

void main(SHADOW_PS_INPUT input) {}