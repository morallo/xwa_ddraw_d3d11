#pragma once

#include "XWAFramework.h"
#include "Matrices.h"
#include "../shaders/material_defs.h"
#include "../shaders/shader_common.h"
#include <vector>

constexpr auto MAX_CACHED_MATERIALS = 32;
constexpr auto MAX_TEXNAME = 40;
constexpr auto MAX_OPT_NAME = 80;
constexpr auto MAX_TEX_SEQ_NAME = 80;
constexpr auto MAX_CANNONS = 8;
constexpr auto MAX_GREEBLE_NAME = 80;
constexpr auto MAX_GREEBLE_LEVELS = 2;
constexpr auto MAX_NORMALMAP_NAME = 80;

typedef enum GreebleBlendModeEnum {
	GBM_MULTIPLY = 1,
	GBM_OVERLAY,
	GBM_SCREEN,
	GBM_REPLACE,
	GBM_NORMAL_MAP,
	GBM_UV_DISP,
	GBM_UV_DISP_AND_NORMAL_MAP,
	// Add more blending modes here, if you do, also update Materials.cpp:g_sGreebleBlendModes
	GBM_MAX_MODES, // Sentinel, do not remove!
} GreebleBlendMode;

extern char *g_sGreebleBlendModes[GBM_MAX_MODES - 1];

typedef enum DiegeticMeshEnum {
	DM_NONE,

	DM_JOYSTICK,

	DM_THR_ROT_X,
	DM_THR_ROT_Y,
	DM_THR_ROT_Z,
	DM_THR_TRANS,
	DM_THR_ROT_ANY,

	DM_HYPER_ROT_X,
	DM_HYPER_ROT_Y,
	DM_HYPER_ROT_Z,
	DM_HYPER_TRANS,
	DM_HYPER_ROT_ANY,
} DiegeticMeshType;

/*

How to add support for a new event:

1. Add the new event in GameEventEnum
2. Add the corresponding string for the new event in g_sGameEventNames
3. Add a new field to g_GameEvent (if applicable).
4. Add support in BeginScene (or wherever relevant) to detect the new event. Add it to g_GameEvent.<Relevant-Field>
5. Update Materials.h:GetCurrentATCIndex(). Mind the priority of the new event!
6. Update Materials.cpp:ResetGameEvent()
7. Update Materials.cpp:UpdateEventsFired()

*/

constexpr int TEXTURE_ATC_IDX = 0;
constexpr int LIGHTMAP_ATC_IDX = 1;
constexpr int MAX_ATC_TYPES = 2;
constexpr int MAX_ALT_EXPLOSIONS = 5;
/* Alternate explosions will be loaded in slots 0..3. The following slot is for DS2 explosions */
constexpr int DS2_ALT_EXPLOSION_IDX = 4;

// Target Event Enum
// This enum can be used to trigger animations or changes in materials
// when target-relate events occur.
// For now, the plan is to use it to change which animated textures are
// displayed.
typedef enum GameEventEnum {
	EVT_NONE = 0,				// Play when no other event is active
	TGT_EVT_SELECTED,			// Something has been targeted
	TGT_EVT_LASER_LOCK,			// Laser is "locked"
	TGT_EVT_WARHEAD_LOCKING,		// Warhead is locking (yellow)
	TGT_EVT_WARHEAD_LOCKED,		// Warhead is locked (red)
	// Cockpit Instrument Damage Events
	CPT_EVT_BROKEN_CMD,
	CPT_EVT_BROKEN_LASER_ION,
	CPT_EVT_BROKEN_BEAM_WEAPON,
	CPT_EVT_BROKEN_SHIELDS,
	CPT_EVT_BROKEN_THROTTLE,
	CPT_EVT_BROKEN_SENSORS,
	CPT_EVT_BROKEN_LASER_RECHARGE,
	CPT_EVT_BROKEN_ENGINE_POWER,
	CPT_EVT_BROKEN_SHIELD_RECHARGE,
	CPT_EVT_BROKEN_BEAM_RECHARGE,
	// Hull Damage Events
	HULL_EVT_DAMAGE_75,
	HULL_EVT_DAMAGE_50,
	HULL_EVT_DAMAGE_25,
	// Warning Light Events
	WLIGHT_EVT_LEFT,
	WLIGHT_EVT_MID_LEFT,
	WLIGHT_EVT_MID_RIGHT,
	WLIGHT_EVT_RIGHT_YELLOW,
	WLIGHT_EVT_RIGHT_RED,
	// Laser/Ion cannon ready events
	CANNON_EVT_1_READY,
	CANNON_EVT_2_READY,
	CANNON_EVT_3_READY,
	CANNON_EVT_4_READY,
	CANNON_EVT_5_READY,
	CANNON_EVT_6_READY,
	CANNON_EVT_7_READY,
	CANNON_EVT_8_READY,
	// How many more cannons can be supported?
	// ... Add more events here
	// End-of-events sentinel. Do not remove!
	MAX_GAME_EVT
} GameEvent;

extern char *g_sGameEventNames[MAX_GAME_EVT];

// *********************
// Cockpit Damage
// *********************
/*
 0x0001 is the CMD/Targeting computer.
 0x000E is the laser/ion display. Looks like all 3 bits must be on, but not sure what happens if the craft doesn't have ions
 0x0010 is the beam weapon
 0x0020 is the shields display
 0x0040 is the throttle (text) display <-- This matches the code that Jeremy provided (see m181 & 64 below)
 0x0180 both sensors. Both bits must be on, if either bit is off, both sensors will shut down
 0x0200 lasers recharge rate
 0x0400 engine level
 0x0800 shields recharge rate
 0x1000 beam recharge rate
*/
typedef union CockpitDamageUnion {
	struct {
		/* 0x0001 */ WORD CMD : 1;
		/* 0x000E */ WORD LaserIon : 3;
		/* 0x0010 */ WORD BeamWeapon : 1;
		/* 0x0020 */ WORD Shields : 1;
		/* 0x0040 */ WORD Throttle : 1;
		/* 0x0180 */ WORD Sensors : 2;
		/* 0x0200 */ WORD LaserRecharge : 1;
		/* 0x0400 */ WORD EngineLevel : 1;
		/* 0x0800 */ WORD ShieldRecharge : 1;
		/* 0x1000 */ WORD BeamRecharge : 1;
	};
	WORD Flags;
} CockpitDamage;

typedef struct CockpitInstrumentStateStruct {
	bool CMD;
	bool LaserIon;
	bool BeamWeapon;
	bool Shields;
	bool Throttle;
	bool Sensors;
	bool LaserRecharge;
	bool EnginePower;
	bool ShieldRecharge;
	bool BeamRecharge;

	CockpitInstrumentStateStruct() {
		CMD = true;
		LaserIon = true;
		BeamWeapon = true;
		Shields = true;
		Throttle = true;
		Sensors = true;
		LaserRecharge = true;
		EnginePower = true;
		ShieldRecharge = true;
		BeamRecharge = true;
	}

	void FromXWADamage(WORD XWADamage);
} CockpitInstrumentState;

typedef struct GlobalGameEventStruct {
	GameEvent TargetEvent;
	CockpitInstrumentState CockpitInstruments, InitialCockpitInstruments;
	bool bCockpitInitialized;
	GameEvent HullEvent;
	// Warning Lights Events
	bool WLightLLEvent;
	bool WLightMLEvent;
	bool WLightMREvent;
	uint8_t WLightRREvent;
	// Cannon Ready Events
	bool CannonReady[MAX_CANNONS];

	GlobalGameEventStruct() {
		TargetEvent = EVT_NONE;
		WLightLLEvent = false;
		WLightMLEvent = false;
		WLightMREvent = false;
		WLightRREvent = 0; // 0: No event, 1: Yellow (locking), 2: Red (locked)
		for (int i = 0; i < MAX_CANNONS; i++)
			CannonReady[i] = false;
		bCockpitInitialized = false;
	}
} GlobalGameEvent;

// Global game event. Updated throughout the frame, reset in Direct3DDevice::BeginScene()
extern GlobalGameEvent g_GameEvent;

// Used to store the information related to animated light maps that
// is loaded from .mat files:
typedef struct TexSeqElemStruct {
	int ExtraTextureIndex, GroupId, ImageId;
	char texname[MAX_TEX_SEQ_NAME];
	float seconds, intensity;
	bool IsDATImage;
	bool IsZIPImage;

	TexSeqElemStruct() {
		ExtraTextureIndex = -1;
		texname[0] = 0;
		IsDATImage = false;
		IsZIPImage = false;
		GroupId = ImageId = -1;
		seconds = intensity = 0.0f;
	}
} TexSeqElem;

typedef struct AnimatedTexControlStruct {
	// Animated LightMaps:
	std::vector<TexSeqElemStruct> Sequence;
	bool SequenceLoaded;
	int AnimIdx; // This is the current index in the Sequence, it can increase monotonically, or it can be random.
	float TimeLeft; // Time left for the current index in the sequence.
	bool IsRandom, BlackToAlpha, NoLoop, AlphaIsBloomMask;
	float4 Tint;
	GameEvent Event; // Activate this animation according to the value set in this field, this is like a "back-pointer" to the event
	float2 Offset;
	float AspectRatio;
	int Clamp;
	
	AnimatedTexControlStruct() {
		Sequence.clear();
		SequenceLoaded = false;
		AnimIdx = 0;
		TimeLeft = 1.0f;
		IsRandom = false;
		BlackToAlpha = false;
		AlphaIsBloomMask = false;
		Tint.x = 1.0f;
		Tint.y = 1.0f;
		Tint.z = 1.0f;
		Event = EVT_NONE;
		Offset.x = 0.0f;
		Offset.y = 0.0f;
		AspectRatio = 1.0f;
		Clamp = 0;
		NoLoop = false; // Animations loop by default
	}

	void ResetAnimation();
	// Updates the timer/index on the current animated material. Only call this function
	// if the current material has an animation.
	void Animate();
} AnimatedTexControl;

// Used to store the information related to greebles
typedef struct GreebleDataStruct {
	// Holds the DAT filename where the greeble data is stored.
	char GreebleTexName[MAX_GREEBLE_LEVELS][MAX_GREEBLE_NAME];
	char GreebleLightMapName[MAX_GREEBLE_LEVELS][MAX_GREEBLE_NAME];
	// The following are indices into resources->_extraTextures. If greeble
	// textures are loaded, these indices will be greater than -1
	int GreebleTexIndex[MAX_GREEBLE_LEVELS];
	int GreebleLightMapIndex[MAX_GREEBLE_LEVELS];
	
	float GreebleDist[MAX_GREEBLE_LEVELS];
	float GreebleLightMapDist[MAX_GREEBLE_LEVELS];
	
	GreebleBlendMode greebleBlendMode[MAX_GREEBLE_LEVELS];
	GreebleBlendMode greebleLightMapBlendMode[MAX_GREEBLE_LEVELS];
	
	float GreebleMix[MAX_GREEBLE_LEVELS];
	float GreebleLightMapMix[MAX_GREEBLE_LEVELS];
	
	float GreebleScale[MAX_GREEBLE_LEVELS];
	float GreebleLightMapScale[MAX_GREEBLE_LEVELS];

	float2 UVDispMapResolution;

	GreebleDataStruct() {
		UVDispMapResolution.x = 1.0;
		UVDispMapResolution.y = 1.0;

		for (int i = 0; i < MAX_GREEBLE_LEVELS; i++) {
			GreebleTexName[i][0] = 0;
			GreebleTexIndex[i] = -1;
			GreebleLightMapName[i][0] = 0;
			GreebleLightMapIndex[i] = -1;
			GreebleDist[i] = 500.0f / (i + 1.0f);
			GreebleLightMapDist[i] = 500.0f / (i + 1.0f);
			greebleBlendMode[i] = GBM_MULTIPLY;
			greebleLightMapBlendMode[i] = GBM_SCREEN;
			GreebleMix[i] = 0.9f;
			GreebleLightMapMix[i] = 0.9f;
			GreebleScale[i] = 1.0f;
			GreebleLightMapScale[i] = 1.0f;
		}
	}
} GreebleData;

// Used to animate sections of an OPT
class MeshTransform {
public:
	Vector3 Center;
	float CurRotX, CurRotY, CurRotZ;
	float RotXDelta, RotYDelta, RotZDelta;
	bool bDoTransform;

	MeshTransform()
	{
		CurRotX = CurRotY = CurRotZ = 0.0f;
		RotXDelta = RotYDelta = RotZDelta = 0.0f;
		bDoTransform = false;
	}

	Matrix4 ComputeTransform();
	void UpdateTransform();
};

// Materials
typedef struct MaterialStruct {
	float Metallic;
	float Intensity;
	float Glossiness;
	float NMIntensity;
	float SpecValue;
	bool  IsShadeless;
	bool  NoBloom;
	Vector3 Light;
	Vector2 LightUVCoordPos;
	bool  IsLava;
	float LavaSpeed;
	float LavaSize;
	float EffectBloom;
	Vector3 LavaColor;
	bool LavaTiling;
	bool AlphaToBloom;
	bool NoColorAlpha; // When set, forces the alpha of the color output to 0
	bool AlphaIsntGlass; // When set, semi-transparent areas aren't translated to a Glass material
	float Ambient;

	int TotalFrames; // Used for animated DAT files, like the explosions

	float ExplosionScale;
	float ExplosionSpeed;
	int ExplosionBlendMode;
	// Holds indices into g_AnimatedMaterials for alternate explosion animations
	int AltExplosionIdx[MAX_ALT_EXPLOSIONS];
	int DS2ExplosionIdx;

	// Set to false by default. Should be set to true once the GroupId 
	// and ImageId have been parsed:
	bool DATGroupImageIdParsed;
	int GroupId;
	int ImageId;

	int TextureATCIndices[MAX_ATC_TYPES][MAX_GAME_EVT];

	//GreebleData GreebleData;
	int GreebleDataIdx;

	DiegeticMeshType DiegeticMesh;
	// If DiegeticMesh == DM_JOYSTICK then the following should be populated:
	Vector3 JoystickRoot;
	float JoystickMaxYaw;
	float JoystickMaxPitch;

	// If DiegeticMesh == DM_THR_ROT_*, then the following should be populated:
	Vector3 ThrottleRoot;
	float ThrottleMinAngle;
	float ThrottleMaxAngle;

	// If DiegeticMesh == DM_THR_TRANS, then the following should be populated
	Vector3 ThrottleStart;
	Vector3 ThrottleEnd;

	// Rotation matrix helper around an arbitrary axis. This matrix will align
	// ThrottleEnd - ThrottleStart with the Z+ axis
	Matrix4 RotAxisToZPlus;
	// If this flag is set, then RotAxisToZPlus has been computed and cached
	bool bRotAxisToZPlusReady;

	MeshTransform meshTransform;

	char NormalMapName[MAX_NORMALMAP_NAME];
	bool NormalMapLoaded;

	// DEBUG properties, remove later
	//Vector3 LavaNormalMult;
	//Vector3 LavaPosMult;
	//bool LavaTranspose;

	MaterialStruct() {
		Metallic = DEFAULT_METALLIC;
		Intensity = DEFAULT_SPEC_INT;
		Glossiness = DEFAULT_GLOSSINESS;
		NMIntensity = DEFAULT_NM_INT;
		SpecValue = DEFAULT_SPEC_VALUE;
		IsShadeless = false;
		Light = Vector3(0.0f, 0.0f, 0.0f);
		LightUVCoordPos = Vector2(0.1f, 0.5f);
		NoBloom = false;
		IsLava = false;
		LavaSpeed = 1.0f;
		LavaSize = 1.0f;
		EffectBloom = 1.0f;
		LavaTiling = true;

		LavaColor.x = 1.00f;
		LavaColor.y = 0.35f;
		LavaColor.z = 0.05f;

		AlphaToBloom = false;
		NoColorAlpha = false;
		AlphaIsntGlass = false;
		Ambient = 0.0f;

		TotalFrames	= 0;
		ExplosionScale = 2.0f; // 2.0f is the original scale
		ExplosionSpeed = 0.001f;
		// MechDonald's explosions are so good that we probably want to default to using them
		// now (instead of blending them with a procedural explosion)
		ExplosionBlendMode = 0; // 0: Original texture, 1: Blend with procedural explosion, 2: Use procedural explosions only

		DATGroupImageIdParsed = false;
		GroupId = 0;
		ImageId = 0;

		for (int j = 0; j < MAX_ATC_TYPES; j++)
			for (int i = 0; i < MAX_GAME_EVT; i++)
				TextureATCIndices[j][i] = -1;

		GreebleDataIdx = -1;

		for (int i = 0; i < MAX_ALT_EXPLOSIONS; i++)
			AltExplosionIdx[i] = -1;
		DS2ExplosionIdx = -1;

		DiegeticMesh = DM_NONE;
		JoystickRoot = Vector3(0, 0, 0);
		JoystickMaxYaw = 10.0f;
		JoystickMaxPitch = -10.0f;
		RotAxisToZPlus.identity();
		bRotAxisToZPlusReady = false;

		NormalMapName[0] = 0;
		NormalMapLoaded = false;

		/*
		// DEBUG properties, remove later
		LavaNormalMult.x = 1.0f;
		LavaNormalMult.y = 1.0f;
		LavaNormalMult.z = 1.0f;

		LavaPosMult.x = -1.0f;
		LavaPosMult.y = -1.0f;
		LavaPosMult.z = -1.0f;
		LavaTranspose = true;
		*/
	}

	// Returns true if any of the possible texture indices is enabled
	inline bool AnyTextureATCIndex() {
		for (int i = 0; i < MAX_GAME_EVT; i++)
			if (TextureATCIndices[TEXTURE_ATC_IDX][i] > -1)
				return true;
		return false;
	}

	// Returns true if any of the possible lightmap indices is enabled
	inline bool AnyLightMapATCIndex() {
		for (int i = 0; i < MAX_GAME_EVT; i++)
			if (TextureATCIndices[LIGHTMAP_ATC_IDX][i] > -1)
				return true;
		return false;
	}

	inline int GetCurrentATCIndex(bool *bIsDamageTex, int ATCType = TEXTURE_ATC_IDX) {
		int index = TextureATCIndices[ATCType][EVT_NONE]; // Default index, this is what we'll play if EVT_NONE is set
		if (bIsDamageTex != NULL) *bIsDamageTex = false;

		// Overrides: these indices are only selected if specific events are set
		// Most-specific events first... least-specific events later.

		// Warning Light Events (this group event is independent from the others above)
		if (g_GameEvent.WLightLLEvent && TextureATCIndices[ATCType][WLIGHT_EVT_LEFT] > -1)
			return TextureATCIndices[ATCType][WLIGHT_EVT_LEFT];
		if (g_GameEvent.WLightMLEvent && TextureATCIndices[ATCType][WLIGHT_EVT_MID_LEFT] > -1)
			return TextureATCIndices[ATCType][WLIGHT_EVT_MID_LEFT];
		if (g_GameEvent.WLightMREvent && TextureATCIndices[ATCType][WLIGHT_EVT_MID_RIGHT] > -1)
			return TextureATCIndices[ATCType][WLIGHT_EVT_MID_RIGHT];
		if (g_GameEvent.WLightRREvent == 1 && TextureATCIndices[ATCType][WLIGHT_EVT_RIGHT_YELLOW] > -1)
			return TextureATCIndices[ATCType][WLIGHT_EVT_RIGHT_YELLOW];
		if (g_GameEvent.WLightRREvent == 2 && TextureATCIndices[ATCType][WLIGHT_EVT_RIGHT_RED] > -1)
			return TextureATCIndices[ATCType][WLIGHT_EVT_RIGHT_RED];

		// Cannon Ready Events
		for (int i = 0; i < MAX_CANNONS; i++)
			if (g_GameEvent.CannonReady[i] && TextureATCIndices[ATCType][CANNON_EVT_1_READY + i] > -1)
				return TextureATCIndices[ATCType][CANNON_EVT_1_READY + i];

		// Cockpit Damage Events
		if (!g_GameEvent.CockpitInstruments.CMD && TextureATCIndices[ATCType][CPT_EVT_BROKEN_CMD] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_CMD];
		}
		
		if (!g_GameEvent.CockpitInstruments.LaserIon && TextureATCIndices[ATCType][CPT_EVT_BROKEN_LASER_ION] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_LASER_ION];
		}

		if (!g_GameEvent.CockpitInstruments.BeamWeapon && TextureATCIndices[ATCType][CPT_EVT_BROKEN_BEAM_WEAPON] > -1 &&
			g_GameEvent.InitialCockpitInstruments.BeamWeapon) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_BEAM_WEAPON];
		}

		if (!g_GameEvent.CockpitInstruments.Shields && TextureATCIndices[ATCType][CPT_EVT_BROKEN_SHIELDS] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_SHIELDS];
		}

		if (!g_GameEvent.CockpitInstruments.Throttle && TextureATCIndices[ATCType][CPT_EVT_BROKEN_THROTTLE] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_THROTTLE];
		}

		if (!g_GameEvent.CockpitInstruments.Sensors && TextureATCIndices[ATCType][CPT_EVT_BROKEN_SENSORS] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_SENSORS];
		}

		if (!g_GameEvent.CockpitInstruments.LaserRecharge && TextureATCIndices[ATCType][CPT_EVT_BROKEN_LASER_RECHARGE] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_LASER_RECHARGE];
		}

		if (!g_GameEvent.CockpitInstruments.EnginePower && TextureATCIndices[ATCType][CPT_EVT_BROKEN_ENGINE_POWER] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_ENGINE_POWER];
		}

		if (!g_GameEvent.CockpitInstruments.ShieldRecharge && TextureATCIndices[ATCType][CPT_EVT_BROKEN_SHIELD_RECHARGE] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_SHIELD_RECHARGE];
		}

		if (!g_GameEvent.CockpitInstruments.BeamRecharge && TextureATCIndices[ATCType][CPT_EVT_BROKEN_BEAM_RECHARGE] > -1 &&
			g_GameEvent.InitialCockpitInstruments.BeamRecharge) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_BEAM_RECHARGE];
		}

		// Hull Damage Events
		if (g_GameEvent.HullEvent == HULL_EVT_DAMAGE_25 && TextureATCIndices[ATCType][HULL_EVT_DAMAGE_25] > -1)
			return TextureATCIndices[ATCType][HULL_EVT_DAMAGE_25];

		if (g_GameEvent.HullEvent == HULL_EVT_DAMAGE_50 && TextureATCIndices[ATCType][HULL_EVT_DAMAGE_50] > -1)
			return TextureATCIndices[ATCType][HULL_EVT_DAMAGE_50];

		if (g_GameEvent.HullEvent == HULL_EVT_DAMAGE_75 && TextureATCIndices[ATCType][HULL_EVT_DAMAGE_75] > -1)
			return TextureATCIndices[ATCType][HULL_EVT_DAMAGE_75];

		// Target Events
		if (g_GameEvent.TargetEvent == TGT_EVT_LASER_LOCK && TextureATCIndices[ATCType][TGT_EVT_LASER_LOCK] > -1)
			return TextureATCIndices[ATCType][TGT_EVT_LASER_LOCK];
		
		if (g_GameEvent.TargetEvent == TGT_EVT_WARHEAD_LOCKING && TextureATCIndices[ATCType][TGT_EVT_WARHEAD_LOCKING] > -1)
			return TextureATCIndices[ATCType][TGT_EVT_WARHEAD_LOCKING];

		if (g_GameEvent.TargetEvent == TGT_EVT_WARHEAD_LOCKED && TextureATCIndices[ATCType][TGT_EVT_WARHEAD_LOCKED] > -1)
			return TextureATCIndices[ATCType][TGT_EVT_WARHEAD_LOCKED];

		// If we're parked in the hangar and this material belongs to the CMD, then we don't want to animate it. The reason
		// is that in the PPG the CMD will display data even when nothing is targeted and we don't want to override it with
		// an animation. Right now, we detect if the current material belong to a CMD by looking at other target-related events.
		// This is reasonable. If cockpit authors only specify EVT_NONE for the CMD, then it will never display the CMD contents.
		// They must also specify at least EVT_SELECTED so that the animation is removed once something is targeted.
		if (*g_playerInHangar && (
			TextureATCIndices[ATCType][TGT_EVT_SELECTED] > -1 ||
			TextureATCIndices[ATCType][TGT_EVT_LASER_LOCK] > -1 ||
			TextureATCIndices[ATCType][TGT_EVT_WARHEAD_LOCKING] > -1 ||
			TextureATCIndices[ATCType][TGT_EVT_WARHEAD_LOCKED] > -1))
		{
			return -1;
		}

		// Least-specific events later.
		// This event is for the TGT_EVT_SELECTED which should be enabled if there's a laser or
		// warhead lock. That's why the condition is placed at the end, after we've checked all
		// the previous events, and we compare against EVT_NONE:
		if (g_GameEvent.TargetEvent != EVT_NONE && TextureATCIndices[ATCType][TGT_EVT_SELECTED] > -1)
			return TextureATCIndices[ATCType][TGT_EVT_SELECTED];

		// Explosion variants cannot be triggered by events. Instead, we need to do a frame-by-frame replacement.
		//if (g_GameEvent.ExplosionEvent != EVT_NONE && TextureATCIndices[ATCType][EVT_DS2] > -1)
		//	TextureATCIndices[ATCType][EVT_DS2];
	
		return index;
	}

} Material;

/*
 Individual entry in the craft material definition file (*.mat). Maintains a copy
 of one entry of the form:

 [TEX000##]
 Metallic   = X
 Reflection = Y
 Glossiness = Z

*/
typedef struct MaterialTexDefStruct {
	Material material;
	char texname[MAX_TEXNAME];
} MaterialTexDef;

/*
 Contains all the entries from a *.mat file for a single craft, along with the
 OPT name for this craft
*/
typedef struct CraftMaterialsStruct {
	std::vector<MaterialTexDef> MaterialList;
	char OPTname[MAX_OPT_NAME];
} CraftMaterials;

typedef struct OPTNameStruct {
	char name[MAX_OPT_NAME];
} OPTNameType;

typedef struct TexnameStruct {
	char name[MAX_TEXNAME];
} TexnameType;

/*
 Contains all the materials for all the OPTs currently loaded
*/
extern std::vector<CraftMaterials> g_Materials;
// List of all materials with animated textures. OPTs may re-use the same
// texture several times in different areas. If this texture has an animation,
// then the animation will be rendered multiple times on the screen. In order
// to update the timer on these animations exactly *once* per frame, we need a
// way to iterate over them at the end of the frame to update their timers.
// This list is used to update the timers on animated materials once per frame.
extern std::vector<AnimatedTexControl> g_AnimatedMaterials;
// List of all the OPTs seen so far
extern std::vector<OPTNameType> g_OPTnames;
// Global Greeble Data (mask, textures, blending modes)
extern std::vector<GreebleData> g_GreebleData;

extern bool g_bReloadMaterialsEnabled;
extern Material g_DefaultGlobalMaterial;

void InitOPTnames();
void ClearOPTnames();
void InitCraftMaterials();
void ClearCraftMaterials();

void OPTNameToMATParamsFile(char* OPTName, char* sFileName, int iFileNameSize);
void DATNameToMATParamsFile(char *DATName, char *sFileName, char *sFileNameShort, int iFileNameSize);
bool LoadIndividualMATParams(char *OPTname, char *sFileName, bool verbose = true);
void ReadMaterialLine(char* buf, Material* curMaterial, char *OPTname);
bool GetGroupIdImageIdFromDATName(char* DATName, int* GroupId, int* ImageId);
void InitOPTnames();
void ClearOPTnames();
void InitCraftMaterials();
void ClearCraftMaterials();
int FindCraftMaterial(char* OPTname);
Material FindMaterial(int CraftIndex, char* TexName, bool debug = false);
// Iterate over all the g_AnimatedMaterials and update their timers
void AnimateMaterials();
// Clears g_AnimatedMaterials
void ClearAnimatedMaterials();

bool ParseDatZipFileNameGroupIdImageId(char *buf, char *sDATFileNameOut, int sDATFileNameSize, short *GroupId, short *ImageId);

void ResetGameEvent();
void UpdateEventsFired();
bool EventFired(GameEvent Event);