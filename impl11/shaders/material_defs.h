#define DEFAULT_METALLIC   0.30f

//#define DEFAULT_SPEC_INT   0.50f
#define DEFAULT_SPEC_INT   0.25f
//#define DEFAULT_GLOSSINESS 0.08f
#define DEFAULT_GLOSSINESS 0.02f
// TODO: Remove NM_INT, this channel is apparently not used anymore
#define DEFAULT_NM_INT     0.5f

#define DEFAULT_SPEC_VALUE 0.0f
// Default material: a bit plasticky; but not completely plastic:
// This value should be DEFAULT_METALLIC / 2.0
#define DEFAULT_MAT        (DEFAULT_METALLIC * 0.5f)

// Material values
// The first channel of SSAOMask is the material. In this channel we have:
// Plastic-Metallicity in the range 0.0..0.5
// Glass: 0.6
// Shadeless: 0.75
// Emission: 1.0
#define PLASTIC_MAT   0.00
#define METAL_MAT     0.50
// TODO: Remove all these:
#define GLASS_MAT     0.60
#define SHADELESS_MAT 0.75
#define EMISSION_MAT  1.00


// Material limits
#define PLASTIC_LO    0.000
#define PLASTIC_HI    0.250

#define METAL_LO      0.250
#define METAL_HI      0.550

// TODO: Remove this:
//#define GLASS_LO      0.575
//#define GLASS_HI      0.625
