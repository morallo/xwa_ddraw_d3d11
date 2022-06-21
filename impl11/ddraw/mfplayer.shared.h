#ifndef MFPLAYER_SHARED_H
#define MFPLAYER_SHARED_H

#include <windows.h>

struct TgSmushSharedData {
	int bVideoPlaybackON; // Written by MFP
	void(*Repaint)();     // Written by ddraw
	void *pDataPtr;
};

extern TgSmushSharedData g_TgSmushSharedData;

// This is a proxy to share data between the hook and ddraw.
// This proxy should only contain two fields:
// - bDataReady is set to true once the writer has initialized
//   the proxy structure.
// - pDataPtr points to the actual data to be shared in the regular
//   heap space
struct TgSmushSharedDataProxy {
	bool bDataReady;
	TgSmushSharedData *pSharedData;
};

constexpr auto TGSMUSH_SHARED_MEM_NAME = L"Local\\TgSmush";
constexpr auto TGSMUSH_SHARED_MEM_SIZE = sizeof(TgSmushSharedDataProxy);

// This is the shared memory handler. Call GetMemoryPtr to get a pointer
// to a SharedData structure
class TgSmushSharedMem {
private:
	HANDLE hMapFile;
	void *pSharedMemPtr;
	bool InitMemory(bool OpenCreate);

public:
	// Specify true in OpenCreate to create the shared memory handle. Set it to false to
	// open an existing shared memory handle.
	TgSmushSharedMem(bool OpenCreate);
	~TgSmushSharedMem();
	void *GetMemoryPtr();
};

#endif