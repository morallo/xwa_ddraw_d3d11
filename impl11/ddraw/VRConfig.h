#pragma once

#include "commonVR.h"

// This value (2.0f) was determined experimentally. It provides an almost 1:1 metric reconstruction when compared with the original models
//const float DEFAULT_FOCAL_DIST = 2.0f; 
const float DEFAULT_METRIC_MULT = 1.0f;
const float DEFAULT_HUD_PARALLAX = 1.7f;
const float DEFAULT_TEXT_PARALLAX = 0.45f;
const float DEFAULT_FLOATING_GUI_PARALLAX = 0.450f;
const float DEFAULT_FLOATING_OBJ_PARALLAX = -0.025f;

const float DEFAULT_TECH_LIB_PARALLAX = -2.0f;
const float DEFAULT_TRIANGLE_POINTER_DIST = 0.120f;
const float DEFAULT_GUI_ELEM_PZ_THRESHOLD = 0.0008f;
const float DEFAULT_ZOOM_OUT_SCALE = 0.5f;
const bool DEFAULT_ZOOM_OUT_INITIAL_STATE = false;
//const float DEFAULT_ASPECT_RATIO = 1.33f;
const float DEFAULT_ASPECT_RATIO = 1.25f;
//const float DEFAULT_CONCOURSE_SCALE = 0.4f;
const float DEFAULT_CONCOURSE_SCALE = 12.0f;
//const float DEFAULT_CONCOURSE_ASPECT_RATIO = 2.0f; // Default for non-SteamVR
const float DEFAULT_CONCOURSE_ASPECT_RATIO = 1.33f; // Default for non-SteamVR
const float DEFAULT_GLOBAL_SCALE = 1.0f;
const float DEFAULT_GUI_SCALE = 0.7f;

const float GAME_SCALE_FACTOR = 60.0f; // Estimated empirically
const float GAME_SCALE_FACTOR_Z = 60.0f; // Estimated empirically

										 /*
const float DEFAULT_LENS_K1 = 2.0f;
const float DEFAULT_LENS_K2 = 0.22f;
const float DEFAULT_LENS_K3 = 0.0f;
*/
const float DEFAULT_LENS_K1 = 3.80f;
const float DEFAULT_LENS_K2 = -0.28f;
const float DEFAULT_LENS_K3 = 100.0f;

//const float DEFAULT_COCKPIT_PZ_THRESHOLD = 0.166f; // I used 0.13f for a long time until I jumped on a TIE-Interceptor
const float DEFAULT_COCKPIT_PZ_THRESHOLD = 10.0f; // De-activated
const int DEFAULT_SKYBOX_INDEX = 2;
const bool DEFAULT_BARREL_EFFECT_STATE = true;
const bool DEFAULT_BARREL_EFFECT_STATE_STEAMVR = false; // SteamVR provides its own lens correction, only enable it if the user really wants it
const float DEFAULT_BRIGHTNESS = 0.95f;
const float MAX_BRIGHTNESS = 1.0f;
const bool DEFAULT_FLOATING_AIMING_HUD = true;
const int DEFAULT_NATURAL_CONCOURSE_ANIM = 1;
const bool DEFAULT_DYNAMIC_COCKPIT_ENABLED = false;
const bool DEFAULT_FIXED_GUI_STATE = true;
// 6dof
const int DEFAULT_FREEPIE_SLOT = 0;
const float DEFAULT_ROLL_MULTIPLIER = -1.0f;
const float DEFAULT_POS_X_MULTIPLIER = 1.0f;
const float DEFAULT_POS_Y_MULTIPLIER = 1.0f;
const float DEFAULT_POS_Z_MULTIPLIER = -1.0f;
const float DEFAULT_MIN_POS_X = -1.5f;
const float DEFAULT_MAX_POS_X = 1.5f;
const float DEFAULT_MIN_POS_Y = -1.5f;
const float DEFAULT_MAX_POS_Y = 1.5f;
const float DEFAULT_MIN_POS_Z = -0.1f;
const float DEFAULT_MAX_POS_Z = 2.5f;
const bool DEFAULT_RESHADE_ENABLED_STATE = false;
const bool DEFAULT_BLOOM_ENABLED_STATE = false;
const int MAX_BLOOM_PASSES = 9;
const int DEFAULT_BLOOM_PASSES = 5;
// TODO: Make this toggleable later
const bool DEFAULT_AO_ENABLED_STATE = false;
// cockpit look constants
const float DEFAULT_YAW_MULTIPLIER = 1.0f;
const float DEFAULT_PITCH_MULTIPLIER = 1.0f;
const float DEFAULT_YAW_OFFSET = 0.0f;
const float DEFAULT_PITCH_OFFSET = 0.0f;
const float DEFAULT_RETICLE_SCALE = 0.8f;

const char* FOCAL_DIST_VRPARAM = "focal_dist";
const char* IPD_VRPARAM = "IPD";
//const char *METRIC_MULT_VRPARAM = "stereoscopy_multiplier";
//const char *SIZE_3D_WINDOW_VRPARAM = "3d_window_size";
const char* SIZE_3D_WINDOW_ZOOM_OUT_VRPARAM = "3d_window_zoom_out_size";
const char* WINDOW_ZOOM_OUT_INITIAL_STATE_VRPARAM = "zoomed_out_on_startup";
const char* CONCOURSE_WINDOW_SCALE_VRPARAM = "concourse_window_scale";
const char* COCKPIT_Z_THRESHOLD_VRPARAM = "cockpit_z_threshold";
//const char *ASPECT_RATIO_VRPARAM = "3d_aspect_ratio";
const char* CONCOURSE_ASPECT_RATIO_VRPARAM = "concourse_aspect_ratio";
const char* K1_VRPARAM = "k1";
const char* K2_VRPARAM = "k2";
const char* K3_VRPARAM = "k3";
const char* HUD_PARALLAX_VRPARAM = "HUD_depth";
const char* GUI_PARALLAX_VRPARAM = "GUI_depth";
const char* GUI_OBJ_PARALLAX_VRPARAM = "GUI_target_relative_depth";
const char* TEXT_PARALLAX_VRPARAM = "Text_depth";
const char* TECH_LIB_PARALLAX_VRPARAM = "Tech_Library_relative_depth";
const char* BRIGHTNESS_VRPARAM = "brightness";
const char* VR_MODE_VRPARAM = "VR_Mode"; // Select "None", "DirectSBS" or "SteamVR"
const char* VR_MODE_NONE_SVAL = "None";
const char* VR_MODE_DIRECT_SBS_SVAL = "DirectSBS";
const char* VR_MODE_STEAMVR_SVAL = "SteamVR";
const char* INTERLEAVED_REPROJ_VRPARAM = "SteamVR_Interleaved_Reprojection";
const char* STEAMVR_DISTORTION_ENABLED_VRPARAM = "steamvr_distortion_enabled";
const char* BARREL_EFFECT_STATE_VRPARAM = "apply_lens_correction";
const char* INVERSE_TRANSPOSE_VRPARAM = "alternate_steamvr_eye_inverse";
const char* FLOATING_AIMING_HUD_VRPARAM = "floating_aiming_HUD";
const char* NATURAL_CONCOURSE_ANIM_VRPARAM = "concourse_animations_at_25fps";
const char* DYNAMIC_COCKPIT_ENABLED_VRPARAM = "dynamic_cockpit_enabled";
const char* FIXED_GUI_VRPARAM = "fixed_GUI";
const char* STICKY_ARROW_KEYS_VRPARAM = "sticky_arrow_keys";
const char* RETICLE_SCALE_VRPARAM = "reticle_scale";
const char* TRIANGLE_POINTER_DIST_VRPARAM = "triangle_pointer_distance";
// 6dof vrparams
const char* ROLL_MULTIPLIER_VRPARAM = "roll_multiplier";
const char* FREEPIE_SLOT_VRPARAM = "freepie_slot";
const char* STEAMVR_POS_FROM_FREEPIE_VRPARAM = "steamvr_pos_from_freepie";
// Cockpitlook params
const char* YAW_MULTIPLIER_CLPARAM = "yaw_multiplier";
const char* PITCH_MULTIPLIER_CLPARAM = "pitch_multiplier";
const char* YAW_OFFSET_CLPARAM = "yaw_offset";
const char* PITCH_OFFSET_CLPARAM = "pitch_offset";

// Dynamic Cockpit vrparams
const char* UV_COORDS_DCPARAM = "uv_coords";
const char* COVER_TEX_NAME_DCPARAM = "cover_texture";
const char* COVER_TEX_SIZE_DCPARAM = "cover_texture_size";
const char* ERASE_REGION_DCPARAM = "erase_region";
const char* MOVE_REGION_DCPARAM = "move_region";
const char* CT_BRIGHTNESS_DCPARAM = "cover_texture_brightness";
const char* DC_TARGET_COMP_UV_COORDS_VRPARAM = "dc_target_comp_uv_coords";
const char* DC_LEFT_RADAR_UV_COORDS_VRPARAM = "dc_left_radar_uv_coords";
const char* DC_RIGHT_RADAR_UV_COORDS_VRPARAM = "dc_right_radar_uv_coords";
const char* DC_SHIELDS_PANEL_UV_COORDS_VRPARAM = "dc_shields_panel_uv_coords";
const char* DC_LASERS_PANEL_UV_COORDS_VRPARAM = "dc_lasers_panel_uv_coords";
const char* DC_FRONT_PANEL_UV_COORDS_VRPARAM = "dc_front_panel_uv_coords";

// This is the current resolution of the screen:
extern float g_fLensK1;
extern float g_fLensK2;
extern float g_fLensK3;

// GUI elements seem to be in the range 0..0.0005, so 0.0008 sounds like a good threshold:
extern float g_fGUIElemPZThreshold ;
extern float g_fTrianglePointerDist ;
extern float g_fGlobalScale ;
//extern float g_fPostProjScale ;
extern float g_fGlobalScaleZoomOut ;
extern float g_fConcourseScale ;
extern float g_fConcourseAspectRatio ;
extern float g_fHUDDepth ; // The aiming HUD is rendered at this depth
extern bool g_bFloatingAimingHUD ; // The aiming HUD can be fixed to the cockpit glass or floating
extern float g_fTextDepth ; // All text gets rendered at this parallax
extern float g_fFloatingGUIDepth ; // Floating GUI elements are rendered at this depth
extern float g_fFloatingGUIObjDepth ; // The targeted object must be rendered above the Floating GUI
extern float g_fTechLibraryParallax ;
extern float g_fAspectRatio ;
extern bool g_bZoomOut ;
extern bool g_bZoomOutInitialState ;
extern float g_fBrightness ;
extern float g_fGUIElemsScale ; // Used to reduce the size of all the GUI elements
extern int g_iFreePIESlot ;
extern int g_iFreePIEControllerSlot ;
extern bool g_bFixedGUI ;
//extern float g_fXWAScale ; // This is the scale value as computed during Execute()

extern float g_fSSAOAlphaOfs;
extern bool g_bDisableDiffuse;
extern bool g_bProceduralLava;

extern float g_fBloomLayerMult[MAX_BLOOM_PASSES + 1];
extern float g_fBloomSpread[MAX_BLOOM_PASSES + 1];
extern int g_iBloomPasses[MAX_BLOOM_PASSES + 1];

extern bool g_bDynCockpitEnabled;



// User-facing functions
void ResetVRParams(); // Restores default values for the view params
void SaveVRParams();
void LoadVRParams();
void LoadCockpitLookParams();
bool LoadGeneric3DCoords(char* buf, float* x, float* y, float* z);
bool LoadGeneric4DCoords(char* buf, float* x, float* y, float* z, float* w);

void ApplyFocalLength(float focal_length);
void SaveFocalLength();
bool LoadFocalLength();

bool LoadDCParams();
bool LoadACParams();
bool LoadBloomParams();
bool LoadSSAOParams();
bool LoadHyperParams();
bool LoadDefaultGlobalMaterial();
void ReloadMaterials();


#ifdef DBG_VR
extern bool g_bDo3DCapture, g_bStart3DCapture;
extern bool g_bCapture2DOffscreenBuffer = false;
extern bool g_bDumpDebug = false;
//extern bool g_bDumpSpecificTex;
//extern int g_iDumpSpecificTexIdx;
void IncreaseCockpitThreshold(float Delta);
void IncreaseNoDrawBeforeIndex(int Delta);
void IncreaseNoDrawAfterIndex(int Delta);
//void IncreaseNoExecIndices(int DeltaBefore, int DeltaAfter);
//void IncreaseZOverride(float Delta);
void IncreaseSkipNonZBufferDrawIdx(int Delta);
void IncreaseNoDrawAfterHUD(int Delta);
#endif


