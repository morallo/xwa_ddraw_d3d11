#include "common.h"
#include "XwaDrawBracketHook.h"
#include "utils.h"

//extern bool g_bGlobalDebugFlag; // g_bInhibitCMDBracket, g_bTargetCompDrawn;

std::vector<XwaBracket> g_xwa_bracket;

void DrawBracketInFlightHook(int A4, int A8, int AC, int A10, unsigned char A14, int A18)
{
	XwaBracket bracket;

	bracket.positionX = A4;
	bracket.positionY = A8;
	bracket.width = AC;
	bracket.height = A10;
	bracket.colorIndex = A14;
	bracket.depth = A18;
	bracket.DC = false;

	if (bracket.depth == 1)
	{
		bracket.positionX += *(short*)0x07D5244;
		bracket.positionY += *(short*)0x07CA354;
	}

	// This helps classify brackets as belonging to the DC. Unfortunately, if the game is
	// set to a low in-game resolution, like 640x480, then some brackets that should be
	// rendered on ships will be misclassified as CMD brackets too because they are too
	// small! Oh well, I can only do so much!
	if (bracket.width <= 4)
		bracket.DC = true;

	//log_debug("[DBG] bracket.width:%d", bracket.width);
	// Looks like CMD brackets always have a width of 4, so that may help in sending them to
	// the DynamicCockpit FG RTV

	//log_debug("[DBG] Bracket Hook");
	/*
		Here's how this hook works:

		[11120][DBG] PRESENT *******************
		[11120][DBG] Bracket Hook
		[11120][DBG](1) Execute() <-- Execute() start
		[11120][DBG](2) Execute() <-- Execute() exit
		[11120][DBG] PRESENT *******************

		In other words: the whole bracket hook is executed *once* before we even start
		rendering anything inside Execute(). How am I supposed to keep track of the state?
		The only way I can tell which brackets should belong to the DC CMD is by looking
		at their size; but there's some danger that they will be misclassified.
	*/

	g_xwa_bracket.push_back(bracket);
}

void DrawBracketMapHook(int A4, int A8, int AC, int A10, unsigned char A14)
{
	XwaBracket bracket;

	bracket.positionX = A4;
	bracket.positionY = A8;
	bracket.width = AC;
	bracket.height = A10;
	bracket.colorIndex = A14;
	bracket.depth = 0;
	bracket.DC = false;

	g_xwa_bracket.push_back(bracket);
}
