#pragma once

#include "dynamic_cockpit.h"

// ACTIVE COCKPIT
#define MAX_AC_COORDS_PER_TEXTURE 64
#define MAX_AC_TEXTURES_PER_COCKPIT 16
#define MAX_AC_ACTION_LEN 8 // WORDs (scan codes) used to specify an action
#define AC_HOLOGRAM_FAKE_VK_CODE 0x01 // Internal AC code to toggle the holograms
typedef struct ac_uv_coords_struct {
	uvfloat4 area[MAX_AC_COORDS_PER_TEXTURE];
	WORD action[MAX_AC_COORDS_PER_TEXTURE][MAX_AC_ACTION_LEN]; // List of scan codes
	char action_name[MAX_AC_COORDS_PER_TEXTURE][16]; // For debug purposes only, remove later
	int numCoords;
} ac_uv_coords;

typedef struct ac_element_struct {
	ac_uv_coords coords;
	//int idx; // "Back pointer" into the g_ACElements array
	char name[MAX_TEXTURE_NAME];
	bool bActive, bNameHasBeenTested;
	short width, height; // DEBUG, remove later
} ac_element;
