#include "mfplayer.shared.h"
#include "utils.h"

TgSmushSharedMem::TgSmushSharedMem(bool OpenCreate) 
{
	InitMemory(OpenCreate);
}

bool TgSmushSharedMem::InitMemory(bool OpenCreate) 
{
	pSharedMemPtr = NULL;

	if (OpenCreate) {
		hMapFile = CreateFileMappingW(
			INVALID_HANDLE_VALUE,    // use paging file
			NULL,                    // default security
			PAGE_READWRITE,          // read/write access
			0,                       // maximum object size (high-order DWORD)
			TGSMUSH_SHARED_MEM_SIZE,         // maximum object size (low-order DWORD)
			TGSMUSH_SHARED_MEM_NAME);        // name of mapping object

		if (hMapFile == NULL)
		{
			return false;
		}
	}
	else {
		hMapFile = OpenFileMappingW(
			FILE_MAP_ALL_ACCESS,   // read/write access
			FALSE,                 // do not inherit the name
			TGSMUSH_SHARED_MEM_NAME);      // name of mapping object

		if (hMapFile == NULL)
		{
			return false;
		}
	}

	pSharedMemPtr = (LPTSTR)MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		TGSMUSH_SHARED_MEM_SIZE);

	if (pSharedMemPtr == NULL) {
		return false;
	}

	log_debug("[DBG] [MFP] TgSmush Shared Mem created by ddraw");
	return true;
}

TgSmushSharedMem::~TgSmushSharedMem()
{
	UnmapViewOfFile(pSharedMemPtr);

	CloseHandle(hMapFile);
}

void *TgSmushSharedMem::GetMemoryPtr()
{
	return pSharedMemPtr;
}