#pragma once

#include "Matrices.h"
#include "../shaders/material_defs.h"
#include "../shaders/shader_common.h"
#include <vector>

constexpr auto MAX_CACHED_MATERIALS = 32;
constexpr auto MAX_TEXNAME = 40;
constexpr auto MAX_OPT_NAME = 80;
constexpr auto MAX_TEX_SEQ_NAME = 80;

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

// Target Event Enum
// This enum can be used to trigger animations or changes in materials
// when target-relate events occur.
// For now, the plan is to use it to change which animated textures are
// displayed.
typedef enum GameEventEnum {
	EVT_NONE = 0,				// Play when no other event is active
	TGT_EVT_SELECTED,			// Something has been targeted
	TGT_EVT_LASER_LOCK,			// Laser is "locked"
	TGT_EVT_WARHEAD_LOCKING,	// Warhead is locking (yellow)
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
	CockpitInstrumentState CockpitInstruments;
	GameEvent HullEvent;

	GlobalGameEventStruct() {
		TargetEvent = EVT_NONE;
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

	TexSeqElemStruct() {
		ExtraTextureIndex = -1;
		texname[0] = 0;
		IsDATImage = false;
		GroupId = ImageId = -1;
		seconds = intensity = 0.0f;
	}
} TexSeqElem;

typedef struct AnimatedTexControlStruct {
	// Animated LightMaps:
	std::vector<TexSeqElemStruct> Sequence;
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
	// Set to false by default. Should be set to true once the GroupId 
	// and ImageId have been parsed:
	bool DATGroupImageIdParsed;
	int GroupId;
	int ImageId;

	int TextureATCIndices[2][MAX_GAME_EVT];

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
		ExplosionBlendMode = 1; // 0: Original texture, 1: Blend with procedural explosion, 2: Use procedural explosions only

		DATGroupImageIdParsed = false;
		GroupId = 0;
		ImageId = 0;

		/*TextureATCIndex = -1;
		TgtEvtSelectedATCIndex = -1;
		TgtEvtLaserLockedATCIndex = -1;
		TgtEvtWarheadLockingATCIndex = -1;
		TgtEvtWarheadLockedATCIndex = -1;

		CptEvtBrokenCMDIndex = -1;
		CptEvtBrokenLaserIonIndex = -1;
		CptEvtBrokenBeamWeaponIndex = -1;
		CptEvtBrokenShieldsIndex = -1;
		CptEvtBrokenThrottleIndex = -1;
		CptEvtBrokenSensorsIndex = -1;
		CptEvtBrokenThrottleIndex = -1;
		CptEvtBrokenLaserRechargeIndex = -1;
		CptEvtBrokenEnginePowerIndex = -1;
		CptEvtBrokenShieldRechargeIndex = -1;
		CptEvtBrokenBeamRechargeIndex = -1;
		*/

		for (int j = 0; j < 2; j++)
			for (int i = 0; i < MAX_GAME_EVT; i++)
				TextureATCIndices[j][i] = -1;

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

		// Cockpit Damage Events
		if (!g_GameEvent.CockpitInstruments.CMD && TextureATCIndices[ATCType][CPT_EVT_BROKEN_CMD] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_CMD];
		}
		
		if (!g_GameEvent.CockpitInstruments.LaserIon && TextureATCIndices[ATCType][CPT_EVT_BROKEN_LASER_ION] > -1) {
			if (bIsDamageTex != NULL) *bIsDamageTex = true;
			return TextureATCIndices[ATCType][CPT_EVT_BROKEN_LASER_ION];
		}

		if (!g_GameEvent.CockpitInstruments.BeamWeapon && TextureATCIndices[ATCType][CPT_EVT_BROKEN_BEAM_WEAPON] > -1) {
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

		if (!g_GameEvent.CockpitInstruments.BeamRecharge && TextureATCIndices[ATCType][CPT_EVT_BROKEN_BEAM_RECHARGE] > -1) {
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

		// Least-specific events later.
		// This event is for the TGT_EVT_SELECTED which should be enabled if there's a laser or
		// warhead lock. That's why the condition is placed at the end, after we've checked all
		// the previous events, and we compare against EVT_NONE:
		if (g_GameEvent.TargetEvent != EVT_NONE && TextureATCIndices[ATCType][TGT_EVT_SELECTED] > -1)
			return TextureATCIndices[ATCType][TGT_EVT_SELECTED];
		
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

extern bool g_bReloadMaterialsEnabled;
extern Material g_DefaultGlobalMaterial;

void InitOPTnames();
void ClearOPTnames();
void InitCraftMaterials();
void ClearCraftMaterials();

void OPTNameToMATParamsFile(char* OPTName, char* sFileName, int iFileNameSize);
void DATNameToMATParamsFile(char *DATName, char *sFileName, char *sFileNameShort, int iFileNameSize);
bool LoadIndividualMATParams(char *OPTname, char *sFileName, bool verbose = true);
void ReadMaterialLine(char* buf, Material* curMaterial);
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

bool ParseDatFileNameGroupIdImageId(char *buf, char *sDATFileNameOut, int sDATFileNameSize, short *GroupId, short *ImageId);

void ResetGameEvent();
void UpdateEventsFired();
bool EventFired(GameEvent Event);