#pragma once
#include "XWAObject.h"
#include "XWAEnums.h"
#include <windows.h>
#include "XWATypes.h"

	// Functions from XWA
	inline const auto CopyRectToFrom = (LONG(*)(RECT * pRectDest, RECT * pRectSrc))0x558CB0;
	inline const auto CreateButtonWithHighlightAndSnd = (int(*)(RECT * rect, char* highlightedImage, char* imageStr, char* hoverText, int highlightColor, int color, int offset, char* soundStr))0x5563C0;
	inline const auto GetFrontTxtString = (char* (*)(int stringIndex))0x55CB50;
	inline const auto LoadSkirmishFile = (int(*)(char* fileStr, int))0x5529B0;
	inline const auto PlayDirectSound = (int(*)(char* soundStr, int, int, int, int, int))0x538850;
	inline const auto LoadMissionsFromLstFile = (char(*)())0x566D90;
	inline const auto SetNumOfPlayerSlotsPerTeam = (int(*)())0x54D620;
	inline const auto FormatFGRoster = (int(*)())0x54D6D0;
	inline const auto DisplayMissionDescription = (void(*)(char*))0x548140;
	inline const auto CreateSelectedButton = (int(*)(RECT * rect, char* imageStr, char* hoverText, int selectedButtonColor))0x5568E0;
	//inline const auto LoadImage = (void(*)(char *imageStr))0x532080;
	inline const auto CombatSimulatorBackgrounds = (int(*)(int))0x54CF90;
	inline const auto SetGlobalStringColorCode = (char(*)(unsigned char))0x434E30;
	inline const auto DP_PlayerIsHost = (int(*)())0x52DD80;
	inline const auto MouseCursorCollidedWithRect = (bool(*)(RECT * pRect, int cursor_X_Pos, int cursor_Y_Pos))0x5592E0;
	inline const auto GetMousePosition = (int(*)(int* cursorX, int* cursorY))0x55BA50;
	inline const auto SyncCombatSimulatorConfig = (bool(*)(int))0x549330;
	inline const auto SetRectDimensions = (RECT * (*)(RECT * pRect, int left, int top, int right, int bottom))0x558C90;
	inline const auto LoadWaveLst = (int(*)(char*, int, int))0x43A150;
	inline const auto AlignRectToDatapad = (int(*)(LONG*))0x558D10;
	inline const auto CreateSettingButton = (int(*)(RECT * rect, char*, char*, int, int, int textColor, int, char* sound))0x556660;
	inline const auto UI_DisplayTextWithRect = (int(*)(int pixels, char* text, RECT * pRect, int textColor))0x5575A0;
	inline const auto ReadSkirmishFile = (int(*)(const char*, int))0x552370;
	inline const auto SyncRoster = (int(*)(int))0x549570;
	//inline const auto sprintf = (int(*)(char* str, const char * format, ...))0x59A680;
	//inline const auto remove = (int(*)(const char *lpFileName))0x59BC30;
	inline const auto LoadSpecificMissionLst = (void(*)(int missionDirectory))0x547800;
	inline const auto UI_CreateTransparentImage = (int(*)(char* resStr, int x, int y))0x532350;
	inline const auto UI_DisplayTextWidthHeight = (int(*)(int pixels, unsigned __int8* text, int width, int height, int textColor))0x557310;
	inline const auto UI_CreateScrollBar = (int(*)(RECT * rect, int scrollPosition, int, int, int, int, int))0x5555D0;
	inline const auto GetCraftSpeciesShortNamePtr = (int(*)(int speciesIndex))0x4DCE50;
	inline const auto UI_CreateClickableText = (int(*)(int, int, int buffer, int, int highlightColor, int, int, char* sound))0x556100;
	inline const auto GetFGPointWorth = (int(*)(int craftType, int fgCreationFlags, int countermeasures, int warheads, int fgAIRank, int fgDuty))0x555420;
	inline const auto GetSpeciesExternalData = (int(*)(int* speciesPtr))0x5552A0;
	inline const auto GetShipBMP = (int(*)(int speciesIndex))0x55DC20;
	inline const auto DisplayShipBMP = (int(*)())0x55DC70;
	inline const auto SetRectWidthHeight = (RECT * (*)(RECT * rect, int width, int height))0x558CD0;
	inline const auto PlayMusic = (int(*)(int musicID))0x49AA40;
	inline const auto StopMusic = (char(*)())0x49ADA0;
	inline const auto AllocateImage = (void** (*)(char* image, char* overlay))0x531D70;
	inline const auto PositionImage = (int(*)(char* image, int height, int width))0x534A60;
	inline const auto DisplayMessage = (int(*)(int messageIndex, int playerIndex))0x497D40;
	inline const auto GetKeyboardDeviceState = (int(*)())0x42B900;
	inline const auto DirectInputKeyboardReaquire = (char(*)())0x42B920;



	// Globals from XWA
	inline const auto missionDirectory = (int*)0xAE2A8A;
	inline const auto implogoStringPtr = (char*)0x60492C;
	inline const auto selectedColor = (int*)0xAE2A30;
	inline const auto normalColor = (int*)0xABD224;
	inline const auto highlightedTextColor = (int*)0xABD228;
	inline const auto highlightedTextColor2 = (int*)0xAE2A48;
	inline const auto combatSimScreenDisplayed = (int*)0x9F4BC8;
	inline const auto rightPanelState = (int*)0x9F4B94;
	inline const auto rightPanelState2 = (int*)0x9F4B48;
	inline const auto leftPanelState = (int*)0x9F4B4C;
	inline const auto configConcourseSFXVolume = (__int8*)0xB0C887;
	inline const auto configDifficulty = (__int8*)0xB0C8BB;
	inline const auto missionDescriptionPtr = *(char**)0x9F4BD0;
	inline const auto missionLstPtr = (int*)0x9F4B98;
	inline const auto missionIndexLoaded = (int*)0x9F5E74;
	inline const auto missionSelectedOnLoadScrn = (int*)0x7830C0;
	inline const auto totalMissionsInLst = (int*)0x9F5EC0;
	inline const auto missionDirectoryMissionSelected = (int*)0xAE2A8E;
	inline const auto missionDescriptionScrollPosition = (int*)0x7831B0;
	//inline const auto rectStandard3 = (int (*)[4])0x6031A8;
	inline RECT* rectStandard3 = (RECT*)0x6031A8;
	inline const auto battleSelectScrollMovement = (int*)0x78317C;
	inline const auto loadScrnTotalMissionsListed = (int*)0x7830BC;
	inline const auto localPlayerIndex = (int*)0x8C1CC8;
	inline ObjectEntry** objects = (ObjectEntry**)0x7B33C4;
	inline PlayerDataEntry* PlayerDataTable = (PlayerDataEntry*)0x8B94E0;
	inline CraftDefinitionEntry* CraftDefinitionTable = (CraftDefinitionEntry*)0x005BB480; // 32 Entries
	inline const auto localPlayerConnectedAs = (int*)0xABD7B4;
	inline const auto flightGroupInfo = (int*)0x783194;
	inline const auto battleSelectMissionScrollIndex = (int*)0x783174;
	inline const auto tacOfficerLoaded = (int*)0x7B6FAE;
	inline const auto battleSelectScrollReturnMovement = (int*)0x783184;
	inline int* FGCreationSlotNum_ = (int*)0x783188;
	inline const auto FGCreationSelectCraftMenu = (int*)0x7830DC;
	inline const auto FGCreationSpecies = (__int16*)0x9F4B82;
	inline const auto FGCreationNumCraft = (__int8*)0x9F4B8D;
	inline const auto FGCreationNumWaves = (__int8*)0x9F4B8C;
	inline const auto provingGrounds = (__int8*)0x8053E5;
	inline const auto tacOfficerInMission = (__int8*)0x7B6FAE;
	inline const auto waveLstString = (int*)0x5B328C;
	inline const auto FGCreationPlayerInSlotTable = (int*)0x9F5EE0;
	inline const auto mainStringBuffer = (char(*)[])0xABD680;
	inline const auto aSS_3 = (char*)0x603394;
	inline const auto skirmishConstStr = (char*)0x603174;
	inline const auto configGoalType = (__int8*)0xB0C8DE;
	inline const auto colorOfLastDisplayedMessage = (__int16*)0x91AC9C;
	inline const auto g_messageColor = (char*)0x8D6C4C;
	inline const auto FGCreationNumOfCraftListed = (int*)0x7830D0;
	inline const auto FGCreationScrollPosition = (int*)0x78319C;
	inline const auto FGCreationCraftIteration = (int*)0x783178;
	inline const auto totalCraftInCraftTable = (int*)0xABD7DC;
	inline const auto speciesShipListPtr = (int*)0xABD22C;
	inline const auto FGCreationPlayerNum = (int*)0x9F4B84;
	inline const auto FGCreationPlayerGunnerNum = (int*)0x9F4B88;
	inline const auto FGCreationGenusSelected = (int*)0x7831A4;
	inline const auto aSS_4 = (char*)0x603548;
	inline const auto aS_0 = (char*)0x5B0E4C;
	inline const auto FGCreationFlags = (int*)0x9F4B90;
	inline const auto FGCreationAIRank = (__int8*)0x9F4B8E;
	inline const auto FGCreationDuty = (__int8*)0x9F4B8F;
	inline const auto FGCreationNumOfWaves = (__int8*)0x9F4B8C;
	inline const auto FGCreationCraftSlotNumTable = (int*)0xABD280;
	inline const auto aD = (char*)0x6012D4;
	inline const auto campaignIncomplete = (int*)0xABCF80;
	inline const auto currentMissionInCampaign = (int*)0xABC970;
	inline const auto currentMusicPlaying = (int*)0xAE2A44;
	inline const auto configDatapadMusic = (__int8*)0xB0C88E;
	inline const auto numberOfPlayersInGame = (int*)0x910DEC;
	inline const auto viewingFilmState = (__int8*)0x80DB68;
	inline const auto mouseLook = (__int8*)0x77129C;
	inline const auto mouseLookWasNotEnabled = (__int8*)0x7712A0;
	inline const auto keyPressedAfterLocaleAfterMapping = (__int16*)0x8053C0;
	inline const auto win32NumPad2Pressed = (__int8*)0x9E9570;
	inline const auto win32NumPad4Pressed = (__int8*)0x9E956B;
	inline const auto win32NumPad5Pressed = (__int8*)0x9E956C;
	inline const auto win32NumPad6Pressed = (__int8*)0x9E956D;
	inline const auto win32NumPad8Pressed = (__int8*)0x9E9568;
	inline const auto inMissionFilmState = (__int8*)0x7D4C4D;
	inline const auto mouseLook_Y = (int*)0x9E9624;
	inline const auto mouseLook_X = (int*)0x9E9620;
	inline const auto mouseLookInverted = (__int8*)0x771298;
	inline const auto mouseLookResetPosition = (int*)0x9E962C;
	inline const auto g_FlightSurfaceHeight = (DWORD*)0x07D4B6C;
	inline auto g_hudScale = (DWORD*)0x06002B8;
	inline uint32_t* g_XwaFlightHudColor = (uint32_t*)0x005B5318; // The current HUD color
	inline uint32_t* g_XwaFlightHudBorderColor = (uint32_t*)0x005B531C; // The current HUD border color

	inline int* s_XwaGlobalLightsCount = (int*)0x00782848;
	inline XwaGlobalLight* s_XwaGlobalLights = (XwaGlobalLight*)0x007D4FA0; // Maximum 8 lights

	inline uint32_t* g_playerInHangar = (uint32_t*)0x09C6E40;
	inline uint32_t* g_playerIndex = (uint32_t*)0x8C1CC8;


	// These values match MXvTED exactly:
	inline const short* g_POV_Y0 = (short*)(0x5BB480 + 0x238);
	inline const short* g_POV_Z0 = (short*)(0x5BB480 + 0x23A);
	inline const short* g_POV_X0 = (short*)(0x5BB480 + 0x23C);
	// Floating-point version of the POV (plus Y is inverted):
	inline const float* g_POV_X = (float*)(0x8B94E0 + 0x20D);
	inline const float* g_POV_Y = (float*)(0x8B94E0 + 0x211);
	inline const float* g_POV_Z = (float*)(0x8B94E0 + 0x215);

	// Alt, Control and Shift key states:
	inline const auto s_XwaIsControlKeyPressed = (int*)0x006343DC;
	inline const auto s_XwaIsShiftKeyPressed = (int*)0x006343E0;
	inline const auto s_XwaIsAltKeyPressed = (int*)0x006343E4;
	// Call the following function to refresh the variables above:
	inline const auto XwaDIKeyboardUpdateShiftControlAltKeysPressedState = (void(*)())0x0042B880;

	// Unknowns
	inline const auto dword_7833D4 = (int*)0x7833D4;
	inline const auto dword_B07C6C = (int*)0xB07C6C;