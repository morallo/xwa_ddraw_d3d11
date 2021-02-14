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
2. Update g_GameEvent.TargetEvent (and other future fields): Add support in BeginScene (or wherever relevant) to detect the new event
3. Add a new TextureATCIndex in MaterialStruct. Initialize it to -1 in the Material constructor
4. Update AnyTextureATCIndex()
5. Update GetCurrentTextureATCIndex()
6. Update Materials.cpp:ParseEvent() and parse the EVT_* label
7. Update Materials.cpp:AssignTextureEvent() and assign into the right index
8. Update Direct3DTexture::TagTexture() and load the frames for the new event

*/
// Target Event Enum
// This enum can be used to trigger animations or changes in materials
// when target-relate events occur.
// For now, the plan is to use it to change which animated textures are
// displayed.
typedef enum GameEventEnum {
	EVT_NONE,					// Play when no other event is active
	TGT_EVT_SELECTED,			// Something has been targeted
	TGT_EVT_LASER_LOCK,			// Laser is "locked"
	TGT_EVT_WARHEAD_LOCKING,	// Warhead is locking (yellow)
	TGT_EVT_WARHEAD_LOCKED		// Warhead is locked (red)
} GameEvent;

// I need to think this more carefully: will there be more events?
typedef struct GlobalGameEventStruct {
	GameEvent TargetEvent;
	// ... More event types?

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
	}
} TexSeqElem;

typedef struct AnimatedTexControlStruct {
	// Animated LightMaps:
	std::vector<TexSeqElemStruct> Sequence;
	int AnimIdx; // This is the current index in the Sequence, it can increase monotonically, or it can be random.
	float TimeLeft; // Time left for the current index in the sequence.
	bool IsRandom, BlackToAlpha;
	float4 Tint;
	GameEvent Event; // Activate this animation according to the value set in this field
	float2 Offset;
	float AspectRatio;
	int Clamp;
	
	AnimatedTexControlStruct() {
		Sequence.clear();
		AnimIdx = 0;
		TimeLeft = 1.0f;
		IsRandom = false;
		BlackToAlpha = false;
		Tint.x = 1.0f;
		Tint.y = 1.0f;
		Tint.z = 1.0f;
		Event = EVT_NONE;
		Offset.x = 0.0f;
		Offset.y = 0.0f;
		AspectRatio = 1.0f;
		Clamp = 0;
	}

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

	int LightMapATCIndex;  // Index into the AnimatedTexControl structure that holds LightMap animation data
	int TextureATCIndex;   // Index into the AnimatedTexControl structure that holds Texture animation data
	int TgtEvtSelectedATCIndex; // Index into AnimatedTexControl structure that holds Texture animation data to be played when a target is selected
	int TgtEvtLaserLockedATCIndex; // Index into animation data to be played when the laser is "locked"
	int TgtEvtWarheadLockingATCIndex; // Index into animation data to be played when a warhead is locking onto a target
	int TgtEvtWarheadLockedATCIndex; // Index into animation data to be played when a warhead is locked

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

		LightMapATCIndex = -1;
		TextureATCIndex = -1;
		TgtEvtSelectedATCIndex = -1;
		TgtEvtLaserLockedATCIndex = -1;
		TgtEvtWarheadLockingATCIndex = -1;
		TgtEvtWarheadLockedATCIndex = -1;

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
		return TextureATCIndex > -1 || TgtEvtSelectedATCIndex > -1 || TgtEvtLaserLockedATCIndex > -1 ||
			TgtEvtWarheadLockingATCIndex > -1 || TgtEvtWarheadLockedATCIndex > -1;
	}

	inline int GetCurrentTextureATCIndex() {
		int index = TextureATCIndex; // Default index, this is what we'll play if EVT_NONE is set

		// Overrides: these indices are only selected if specific events are set
		
		// Most-specific events first...
		if (g_GameEvent.TargetEvent == TGT_EVT_LASER_LOCK && TgtEvtLaserLockedATCIndex > -1)
			return TgtEvtLaserLockedATCIndex;
		
		if (g_GameEvent.TargetEvent == TGT_EVT_WARHEAD_LOCKING && TgtEvtWarheadLockingATCIndex > -1)
			return TgtEvtWarheadLockingATCIndex;

		if (g_GameEvent.TargetEvent == TGT_EVT_WARHEAD_LOCKED && TgtEvtWarheadLockedATCIndex > -1)
			return TgtEvtWarheadLockedATCIndex;

		// Least-specific events later.
		// This event is for the TGT_EVT_SELECTED which should be enabled if there's a laser or
		// warhead lock. That's why the condition is placed at the end, after we've checked all
		// the previous events, and we compare against EVT_NONE:
		if (g_GameEvent.TargetEvent != EVT_NONE && TgtEvtSelectedATCIndex > -1)
			return TgtEvtSelectedATCIndex;
		
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