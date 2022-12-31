#pragma once
#include <windows.h>

class HiResTimer {
public:
	LARGE_INTEGER PC_Frequency, curT, lastT, elapsed_us, start_time;
	float global_time_s, elapsed_s, last_time_s;

	void ResetGlobalTime();
	//float GetElapsedTimeSinceLastCall();
	float GetElapsedTime();
};
extern HiResTimer g_HiResTimer;
