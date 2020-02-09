#define DEFAULT_METALLIC   0.50f
#define DEFAULT_SPEC_INT   0.50f
#define DEFAULT_GLOSSINESS 0.08f
// Default material, neither plastic nor metal; but halfway through:
#define DEFAULT_MAT     0.25

// Material values
#define PLASTIC_MAT		0.00
#define METAL_MAT		0.50
#define GLASS_MAT		0.60
#define SHADELESS_MAT	0.75
#define EMISSION_MAT		1.00


// Material limits
#define PLASTIC_LO		0.000
#define PLASTIC_HI		0.250

#define METAL_LO			0.250
#define METAL_HI			0.550

#define GLASS_LO		    0.550
#define GLASS_HI			0.625

#define SHADELESS_LO	    0.625
#define SHADELESS_HI		0.875

#define EMISSION_LO		0.875
#define EMISSION_HI		1.000
