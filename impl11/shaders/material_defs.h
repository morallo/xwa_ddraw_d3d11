#define DEFAULT_METALLIC   0.30f

//#define DEFAULT_SPEC_INT   0.50f
#define DEFAULT_SPEC_INT   0.25f
//#define DEFAULT_GLOSSINESS 0.08f
#define DEFAULT_GLOSSINESS 0.02f
#define DEFAULT_NM_INT     0.5f

#define DEFAULT_SPEC_VALUE 0.0f
// Default material: a bit plasticky; but not completely plastic:
// This value should be DEFAULT_METALLIC / 2.0
#define DEFAULT_MAT		  (DEFAULT_METALLIC * 0.5f)

// Material values
// The first channel of SSAOMask is the material. In this channel we have:
// Plastic-Metallicity in the range 0.0..0.5
// Glass: 0.6
// Shadeless: 0.75
// Emission: 1.0
#define PLASTIC_MAT		0.00
#define METAL_MAT		0.50
#define GLASS_MAT		0.60
#define SHADELESS_MAT	0.75
#define EMISSION_MAT	1.00


// Material limits
#define PLASTIC_LO		0.000
#define PLASTIC_HI		0.250

#define METAL_LO		0.250
#define METAL_HI		0.550

#define GLASS_LO		0.550
#define GLASS_HI		0.625

#define SHADELESS_LO	0.625
#define SHADELESS_HI	0.875

#define EMISSION_LO		0.875
#define EMISSION_HI		1.000

#define MAX_XWA_LIGHTS 8
// Currently only used for the lasers
#define MAX_CB_POINT_LIGHTS 8
