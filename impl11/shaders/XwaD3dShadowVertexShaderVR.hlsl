/*
 * This is a very wasteful way of doing shadows. We should remove this shader later.
 * This approach renders the shadows twice (once per eye). There's no need to do this.
 * We should instead render a regular shadow map to a smaller texture *once*, and then
 * we should do regular depth testing to render the shadows. This is a simple copy-paste
 * of XwaD3dShadowVertexShader just to move things forward, but we can totally improve
 * this code.
 */
#include "XwaD3dCommon.hlsl"
#include "VertexShaderCBuffer.h"
#include "shader_common.h"
#include "metric_common.h"

Buffer<float3> g_vertices : register(t0);
Buffer<float3> g_normals : register(t1);
Buffer<float2> g_textureCoords : register(t2);

struct VertexShaderInput
{
	int4 index : POSITION;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	//float4 position : COLOR2;
	//float4 normal : COLOR1;
	//float2 tex : TEXCOORD;
	//float4 color : COLOR0;
};

// VertexShaderMatrixCB
cbuffer ConstantBuffer : register(b2)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	float3 v = g_vertices[input.index.x];

	// This is the same (?) transformWorldView we use in the XwaD3dVertexShader.
	// So this transforms from OPT to WorldView. In othe words, it places the OPT
	// on the same position where the objects are sitting in the hangar.
	output.pos = mul(float4(v, 1.0f), transformWorldView);
	// Now we transform from hangar to shadow-camera view. This is a rotation matrix,
	// look at UpdateConstantBuffer() and notice that only the 3x3 upper-left entries are
	// filled. There's no translation...
	output.pos = mul(hangarShadowView, output.pos);
	// ... so here we apply the translation:
	output.pos.x += cameraPositionX;
	output.pos.y += cameraPositionY;
	// We're now in "shadow-camera" space. This coordinate system is from the point of view
	// of the light casting the shadow. In this space, the X and Y axes are the position of
	// the camera and the Z axis is aligned with the camera. So we're looking up-down while
	// we're here.

	// NOTE: The block that does clipping in XwaD3dShadowVertexShader was removed for this
	//		 version.

	// Invert the translation of the shadow-camera
	output.pos.x -= cameraPositionX;
	output.pos.y -= cameraPositionY;
	// Squash the Z coordinate. This is now a flat "shadow". (We could've probably squashed
	// the shadow before inverting the camera translation, but it doesn't matter).
	output.pos.z = floorLevel + 1; // Adding a little shadow bias to prevent artifacts

	// This inverts the hangar-to-shadow-camera transform we did at the beginning of this
	// shader. Notice how the order of the operands is inverted, so it's equivalent to:
	// mul(hangarShadowView.transpose(), output.pos)
	// if transformView is a rotation matrix, then its transpose is the same as the inversion
	output.pos = mul(output.pos, hangarShadowView);
	// We now have a flattened object in the hangar coordinate system, at OPT scale. We can now
	// project to the camera plane	
	// For VR, this is where we would transform from OPT to metric scale, and instead of using
	// TransformProjection, we would need to apply the SteamVR eye-to-camera transform
	// Make a copy of the original 3D -> 2D transform, we'll need it for the depth buffer
	float4 Q = TransformProjection(output.pos.xyz);
	// Convert pos3D from OPT scale to metric scale
	output.pos *= 0.024414; // 1/40.96, OPT to metric conversion factor
	output.pos.y = -output.pos.y; // Further conversion of the coordinate system (Y+ is up)
	output.pos.z = -output.pos.z; // Adjust the coordinate system for SteamVR:
	// Apply the head position and project 3D --> 2D
	if (bFullTransform < 0.5f)
		output.pos = mul(viewMatrix, output.pos);
	else
		output.pos = mul(fullViewMatrix, output.pos);
	output.pos = mul(projEyeMatrix, output.pos);
	// Fix the depth
	output.pos.z = (Q.z / Q.w) * output.pos.w;

	return output;
}
