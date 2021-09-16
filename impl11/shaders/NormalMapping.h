#ifndef NORMAL_MAPPING_H
#define NORMAL_MAPPING_H

/*

Simple implementation of Normal Mapping, adapted from: http://www.thetenthplanet.de/archives/1180

Call perturb_normal(float3 Normal, float3 ViewVector, float2 texcoord, float3 NormalMapCol)

For ViewVector use -Pos3D, this vector doesn't need to be normalized

*/


float3x3 cotangent_frame(float3 N, float3 p, float2 uv)
{
	/*
	I made a little mess when building the normal and depth buffers.

	The depth buffer's coord sys looks like this:
		X+: Right
		Y+: Up
		Z+: Away from the camera

	But the normal buffer looks like this:
		X+: Right
		Y+: Up
		Z+: Pointing directly into the camera

	These systems are inconsistent: the Z coordinate is flipped between these two systems. According
	to the structure of the depth buffer, Z+ should point away from the camera, but in the normal buffer Z+
	points into the camera.

	I did this on purpose, I flipped the Z coordinate of the normals so that blue meant the surface was pointing
	into the camera (I also flipped the Y coordinate, but that's irrelevant because Y is consistent with the
	depth buffer). This made it easier to debug the normal buffer because it looks exactly like a regular normal
	map.

	Now, as a consequence, I have to flip the Z coordinate of the normals in the computations below, so that they
	are consistent with the depth buffer and the 3D position (p). Then at the end of all the math, I have to flip
	Z again so that it's consistent with the rest of the code that expects Z+ to point into the camera. Z is
	flipped again at the end of perturb_normal().
	*/
	N.z = -N.z;
	// get edge vectors of the pixel triangle
	float3 dp1 = ddx(p);
	float3 dp2 = ddy(p);
	float2 duv1 = ddx(uv);
	float2 duv2 = ddy(uv);

	// solve the linear system
	float3 dp2perp = cross(dp2, N);
	float3 dp1perp = cross(N, dp1);
	float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = rsqrt(max(dot(T, T), dot(B, B)));
	return float3x3(T * invmax, B * invmax, N);
}

float3 perturb_normal(float3 N, float3 V, float2 texcoord, float3 NMCol)
{
	// N: the interpolated vertex normal and
	// V: the view vector (vertex to eye)
	// NMCol: the raw sampled color from the blue-ish normal map.
	//NMCol.rgb = normalize((NMCol.rgb - 0.5) * 2.0); // Looks like we don't actually need to normalize this
	NMCol.rgb = (NMCol.rgb - 0.5) * 2.0;

	float3x3 TBN = cotangent_frame(N, -V, texcoord); // V is the view vector, so -V is equivalent to the vertex 3D pos
	//return normalize(mul(NMCol, TBN));
	// We flip the Z coordinate for... reasons. See cotangent_frame() for the explanation.
	// I _think_ mul(float3, float3x3) is the right order because of the way the matrix was built.
	float3 tempN = normalize(mul(NMCol, TBN));
	tempN.z = -tempN.z;
	return tempN;
}

#endif