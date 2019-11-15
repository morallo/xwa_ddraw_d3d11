#define vec4 float4
#define vec3 float3
#define vec2 float2

#define bvec2 bool2
#define bvec3 bool3
#define bvec4 bool4

#define mat4 float4x4

#define mix lerp
#define fract frac

#define mod(x, y) (x % y)

cbuffer ConstantBuffer : register(b7)
{
	float3 iMouse;
	float  iTime;
	// 16 bytes
	float2 iResolution;
	float unused1, unused2;
	// 32 bytes
}

