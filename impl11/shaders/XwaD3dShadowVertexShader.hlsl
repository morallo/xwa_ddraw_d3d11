/*
 * This is the original Vertex Shader used for hangar shadows as computed by
 * XWA. It's used with the D3dHook. This is *not* a shadow mapping shader,
 * as it doesn't use the depth buffer for shadowing. It simply renders the
 * hangar scene from a camera looking down.
 */
#include "XwaD3dCommon.hlsl"

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

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	float3 v = g_vertices[input.index.x];
	//float3 n = g_normals[input.index.y];
	//float2 t = g_textureCoords[input.index.z];
	//int c = input.index.w;

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

	// This block clips the current triangle to the limits (sx1,sy1)-(sx2,sy2)
	/*
	if (hangarShadowAccStartEnd != 0.0f)
	{
		if (output.pos.x < sx1)
		{
			if (output.pos.y > sy1)
			{
				output.pos.y = sy1;
			}
		}
		else if (output.pos.x > sx2)
		{
			if (output.pos.y > sy2)
			{
				output.pos.y = sy2;
			}
		}
		else
		{
			// This is a linear interpolation, it linearly maps x in [sx1..sx2] to sy1..sy2
			// depending on where x is.
			// - If x == sx1, then we get sy1.
			// - If x == sx2, then we get sy2.
			float curY = (output.pos.x - sx1) / (sx2 - sx1) * (sy2 - sy1) + sy1;

			if (output.pos.y > curY)
			{
				output.pos.y = curY;
			}
		}
	}
	*/

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
	// project to the camera plane:
	output.pos = TransformProjection(output.pos.xyz);

	// and that's how we create shadows in this game!

	//output.position = float4(v, 1.0f);
	//output.normal = mul(float4(n, 1.0f), transformWorldView);
	//output.tex = t;

	//output.color = float4(1.0f, 1.0f, 1.0f, 0.0f);

	//if (c == 0)
	//{
	//	output.color = ComputeColorLight(v, n);
	//}

	return output;
}
