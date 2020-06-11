// Scale factor used to reconstruct metric 3D for VR and other uses
#define METRIC_SCALE_FACTOR 25.0
//#define METRIC_SCALE_FACTOR 50.0

// The following 2 should be the same; but I want to see if the focal dist = 1
// causes distortions in VR.
#define DEFAULT_FOCAL_DIST 2.0
#define DEFAULT_FOCAL_DIST_VR 1.0

// This is the limit, in meters, when we start fading out effects like SSAO and SSDO:
#define INFINITY_Z0 10000 // Used to be 15000 in release 1.1.2
// This is the limit, in meters, when the SSAO/SSDO effects are completely faded out
#define INFINITY_Z1 20000
// This is simply INFINITY_Z1 - INFINITY_Z0: It's used to fade the effect
#define INFINITY_FADEOUT_RANGE 10000 // Used to be 5000 in release 1.1.2n

// Dynamic Cockpit: Maximum Number of DC elements per texture
#define MAX_DC_COORDS_PER_TEXTURE 12

// Maximum sun flares (not supported yet)
#define MAX_SUN_FLARES 4

#define MAX_XWA_LIGHTS 8
// Currently only used for the lasers
#define MAX_CB_POINT_LIGHTS 8

// Used in the special_control CB field in the pixel shader
#define SPECIAL_CONTROL_XWA_SHADOW 1
#define SPECIAL_CONTROL_GLASS 2

// Register slot for the metric reconstruction constant buffer
#define METRIC_REC_CB_SLOT 6

