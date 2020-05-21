// Scale factor used to reconstruct metric 3D for VR and other uses
#define METRIC_SCALE_FACTOR 25.0
//#define METRIC_SCALE_FACTOR 50.0

// This is the limit, in meters, when we start fading out effects like SSAO and SSDO:
#define INFINITY_Z0 10000 // Used to be 15000 in release 1.1.2
// This is the limit, in meters, when the SSAO/SSDO effects are completely faded out
#define INFINITY_Z1 20000
// This is simply INFINITY_Z1 - INFINITY_Z0: It's used to fade the effect
#define INFINITY_FADEOUT_RANGE 10000 // Used to be 5000 in release 1.1.2n

// Dynamic Cockpit: Maximum Number of DC elements per texture
#define MAX_DC_COORDS_PER_TEXTURE 12

#define MAX_SUN_FLARES 4

// Z Far value (in meters) used for the shadow maps
#define SM_Z_FAR 25.0
