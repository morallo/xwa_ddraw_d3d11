#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

/*

Constant Buffer Registers

PixelShaderCBuffer and several others : register(b0)
VertexShaderMatrixCB and several others : register(b1)
BloomPixelShaderCBuffer : register(b2)
SSAOPixelShaderCBuffer : register(b3)
PSShadingSystemCB : register(b4)
ShadowMapVertexShaderMatrixCB : register(b5)
MetricReconstructionCB : register(b6)
ShadertoyCBuffer : register(b7)
LaserPointerCBuffer : register(b8)

*/

// Scale factor used to reconstruct metric 3D for VR and other uses
// This is now OBSOLETE in the VR path. Only used in non-VR mode and this will probably
// be removed later as well
#define METRIC_SCALE_FACTOR 25.0

// Only used in the non-VR path, and it should be removed later once the non-VR path
// is fully metric as well.
#define DEFAULT_FOCAL_DIST 2.0

// This is the limit, in meters, when we start fading out effects like SSAO and SSDO:
#define INFINITY_Z0 10000 // Used to be 15000 in release 1.1.2
// This is the limit, in meters, when the SSAO/SSDO effects are completely faded out
#define INFINITY_Z1 20000
// This is simply INFINITY_Z1 - INFINITY_Z0: It's used to fade the effect
#define INFINITY_FADEOUT_RANGE 10000 // Used to be 5000 in release 1.1.2

// Dynamic Cockpit: Maximum Number of DC elements per texture
#define MAX_DC_COORDS_PER_TEXTURE 12

// Maximum sun flares (not supported yet)
#define MAX_SUN_FLARES 4

#define MAX_XWA_LIGHTS 8
// Currently only used for the lasers
#define MAX_CB_POINT_LIGHTS 8

// Used in the special_control CB field in the pixel shader
#define SPECIAL_CONTROL_XWA_SHADOW	1
#define SPECIAL_CONTROL_GLASS		2
#define SPECIAL_CONTROL_BACKGROUND	3
#define SPECIAL_CONTROL_SMOKE		4
#define SPECIAL_CONTROL_HIGHLIGHT	5 // Used for debugging purposes (to highlight specific elements)

// Register slot for the metric reconstruction constant buffer
#define METRIC_REC_CB_SLOT 6

// If this value is too high, then DX will fail when creating textures.
// 2^9 = 512, that's probably the smallest dimension the desktop can be
// (either width or height)
// Or we can use 0 when creating the textures and -1 when creating the SRVs
// that seems to work without specifying the maximum mip levels.
#define MAX_MIP_LEVELS 9

// Labels defined at the C++ preprocessor level are not seen in shaders. Therefore,
// we have to define them here so that both C++ files and HLSL files can see these
// definitions.
//#define GENMIPMAPS

// Enable the following define and look at any sun to trigger the SunFlareShader.
// That'll show mip-map level 5 -- it's a nice way to debug mip maps.
//#define GENMIPMAPS_DEBUG

#endif