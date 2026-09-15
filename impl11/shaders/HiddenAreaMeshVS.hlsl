// Hidden Area Mesh Vertex Shader
// Renders the VR hidden area mesh to the depth buffer at z=0 (nearest)
// to enable early-z rejection of pixels never visible through the HMD lenses.
// Uses instanced stereo: instance 0 = left eye, instance 1 = right eye.

struct VertexShaderInput
{
	float2 pos    : POSITION;  // UV-space position from OpenVR [0,1]
	uint   instId : SV_InstanceID;
};

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	uint   viewId : SV_RenderTargetArrayIndex;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	// Convert from OpenVR UV space [0,1] to NDC [-1,1]
	// OpenVR: (0,0) = top-left, (1,1) = bottom-right
	// NDC:    (-1,1) = top-left, (1,-1) = bottom-right
	float x = input.pos.x * 2.0 - 1.0;
	float y = 1.0 - input.pos.y * 2.0;  // Flip Y: UV top=0 -> NDC top=+1

	// z=0.0 is the nearest depth value for D32_FLOAT with LESS comparison.
	// All subsequent geometry with z > 0 will fail depth test against this.
	output.pos = float4(x, y, 0.0, 1.0);

	// Route to the correct render target array slice (left=0, right=1)
	output.viewId = input.instId;

	return output;
}
