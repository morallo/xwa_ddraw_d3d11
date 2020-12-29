#include "Effects.h"
#include "common.h"
#include "XWAFramework.h"

// Main Pixel Shader constant buffer
MainShadersCBuffer			g_MSCBuffer;
// Constant Buffers
BloomPixelShaderCBuffer		g_BloomPSCBuffer;
PSShadingSystemCB			g_ShadingSys_PSBuffer;
SSAOPixelShaderCBuffer		g_SSAO_PSCBuffer;

std::vector<ColorLightPair> g_TextureVector;
/*
 * Used to store a list of textures for fast lookup. For instance, all suns must
 * have their associated lights reset after jumping through hyperspace; and all
 * textures with materials can be placed here so that material properties can be
 * applied while flying.
 */
std::vector<Direct3DTexture*> g_AuxTextureVector;

std::vector<char*> Text_ResNames = {
	"dat,16000,"
};

std::vector<char*> Floating_GUI_ResNames = {
	"dat,12000,2400,", // 0xd08b4437, (16x16) Laser charge. (master branch)
	"dat,12000,2300,", // 0xd0168df9, (64x64) Laser charge boxes.
	"dat,12000,2500,", // 0xe321d785, (64x64) Laser and ion charge boxes on B - Wing. (master branch)
	"dat,12000,2600,", // 0xca2a5c48, (8x8) Laser and ion charge on B - Wing. (master branch)
	"dat,12000,1100,", // 0x3b9a3741, (256x128) Full targetting computer, solid. (master branch)
	"dat,12000,100,",  // 0x7e1b021d, (128x128) Left targetting computer, solid. (master branch)
	"dat,12000,200,",  // 0x771a714,  (256x256) Left targetting computer, frame only. (master branch)
};

// List of regular GUI elements (this is not an exhaustive list). It's mostly used to detect when
// the game has started rendering the GUI
std::vector<char*> GUI_ResNames = {
	"dat,12000,2700,", // 0xc2416bf9, // (256x32) Top-left bracket (master branch)
	"dat,12000,2800,", // 0x71ce88f1, // (256x32) Top-right bracket (master branch)
	"dat,12000,4500,", // 0x75b9e062, // (128x128) Left radar (master branch)
	"dat,12000,300,",  // (128x128) Left sensor when... no shields are present?
	"dat,12000,4600,", // 0x1ec963a9, // (128x128) Right radar (master branch)
	"dat,12000,400,",  // 0xbe6846fb, // Right radar when no tractor beam is present
	"dat,12000,4300,", // 0x3188119f, // (128x128) Left Shield Display (master branch)
	"dat,12000,4400,", // 0x75082e5e, // (128x128) Right Tractor Beam Display (master branch)
};

// List of common roots for the electricity names
std::vector<char*> Electricity_ResNames = {
	// Animations (electricity)
	"dat,2007,",
	"dat,2008,",
	"dat,3005,",
	//"dat,3051,", // Hyperspace!

	// Sparks
	"dat,3000,",
	"dat,3001,",
	"dat,3002,",

	// Backdrops
	/*"dat,9001,",
	"dat,9002,",
	"dat,9003,",
	"dat,9004,",
	"dat,9005,",
	"dat,9006,",
	"dat,9007,",
	"dat,9008,",
	"dat,9009,",
	"dat,9010,",
	"dat,9100,",*/
	// Particles
	"dat,22000,",
	"dat,22003,",
	"dat,22005,",
	//"dat,22007,", // Cockpit sparks
};

// List of common roots for the Explosion names
std::vector<char*> Explosion_ResNames = {
	// Explosions (these are all animated)
	"dat,2000,",
	"dat,2001,",
	"dat,2002,",
	"dat,2003,",
	"dat,2004,",
	"dat,2005,",
	"dat,2006,",
};

// Smoke from explosions:
std::vector<char*> Smoke_ResNames = {
	"dat,3003,",
	"dat,3004,",
	// The following used to be tagged as explosions, but they look like smoke
	"dat,3006,",
	"dat,3055,",
	"dat,3100,",
	"dat,3200,",
	"dat,3300,",
	"dat,3400,",
	"dat,3500,",
};

std::vector<char*> Sparks_ResNames = {
	"dat,3000,",
	"dat,3001,",
	"dat,3002,",
	"dat,3003,",
	"dat,3004,",
};

// List of Lens Flare effects
std::vector<char*> LensFlare_ResNames = {
	"dat,1000,3,",
	"dat,1000,4,",
	"dat,1000,5,",
	"dat,1000,6,",
	"dat,1000,7,",
	"dat,1000,8,",
};

// List of Suns in the Backdrop.dat file
std::vector<char*> Sun_ResNames = {
	"dat,9001,",
	"dat,9002,",
	"dat,9003,",
	"dat,9004,",
	"dat,9005,",
	"dat,9006,",
	"dat,9007,",
	"dat,9008,",
	"dat,9009,",
	"dat,9010,",
};

/*
// With a few exceptions, all planets are in the dat,6XXX, series. It's
// probably more efficient to parse that algorithmically.
std::vector<char *> Planet_ResNames = {
	"dat,6010,",
	"dat,6010,",
	...
};
*/

std::vector<char*> SpaceDebris_ResNames = {
	"dat,4000,",
	"dat,4001,",
	"dat,4002,",
	"dat,4003,"
};

std::vector<char*> Trails_ResNames = {
	"dat,21000,",
	"dat,21005,",
	"dat,21010,",
	"dat,21015,",
	"dat,21020,",
	"dat,21025,",
};

FOVtype g_CurrentFOVType = GLOBAL_FOV;
FOVtype g_CurrentFOV = GLOBAL_FOV;

// Global y_center and FOVscale parameters. These are updated only in ComputeHyperFOVParams.
float g_fYCenter = 0.0f, g_fFOVscale = 0.75f;
Vector2 g_ReticleCentroid(-1.0f, -1.0f);
bool g_bTriggerReticleCapture = false, g_bYCenterHasBeenFixed = false;

float g_fRealHorzFOV = 0.0f; // The real Horizontal FOV, in radians
float g_fRealVertFOV = 0.0f; // The real Vertical FOV, in radians

Vector3 g_LaserPointDebug(0.0f, 0.0f, 0.0f);
Vector3 g_HeadLightsPosition(0.0f, 0.0f, 20.0f), g_HeadLightsColor(0.85f, 0.85f, 0.90f);
float g_fHeadLightsAmbient = 0.05f, g_fHeadLightsDistance = 5000.0f, g_fHeadLightsAngleCos = 0.25f; // Approx cos(75)
bool g_bHeadLightsAutoTurnOn = true;

D3DTLVERTEX g_SpeedParticles2D[MAX_SPEED_PARTICLES * 12];

void SmallestK::insert(Vector3 P, Vector3 col) {
	int i = _size - 1;
	while (i >= 0 && P.z < _elems[i].P.z) {
		// Copy the i-th element to the (i+1)-th index to make space at i
		if (i + 1 < MAX_CB_POINT_LIGHTS)
			_elems[i + 1] = _elems[i];
		i--;
	}

	// Insert at i + 1 (if possible) and we're done
	if (i + 1 < MAX_CB_POINT_LIGHTS) {
		_elems[i + 1].P = P;
		_elems[i + 1].col = col;
		if (_size < MAX_CB_POINT_LIGHTS)
			_size++;
	}
}

bool isInVector(uint32_t crc, std::vector<uint32_t>& vector) {
	for (uint32_t x : vector)
		if (x == crc)
			return true;
	return false;
}

bool isInVector(char* name, std::vector<char*>& vector) {
	for (char* x : vector)
		if (stristr(name, x) != NULL)
			return true;
	return false;
}

bool isInVector(char* OPTname, std::vector<OPTNameType>& vector) {
	for (OPTNameType x : vector)
		if (_stricmp(OPTname, x.name) == 0) // We need to avoid substrings because OPTs can be "Awing", "AwingExterior", "AwingCockpit"
			return true;
	return false;
}