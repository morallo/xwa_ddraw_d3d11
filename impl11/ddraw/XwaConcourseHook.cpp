#include "common.h"
#include "XwaConcourseHook.h"
#include "DeviceResources.h"
#include "PrimarySurface.h"
#include "FrontbufferSurface.h"
#include "DirectDraw.h"
#include <fstream>

std::wstring BuildScreenshotFilename(const std::wstring& prefix)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	wchar_t filename[MAX_PATH];

	wsprintfW(
		filename,
		L"%s\\%s_%4d%02d%02d_%02d%02d%02d%03d.jpg",
		string_towstring(g_config.ScreenshotsDirectory).c_str(),
		prefix.c_str(),
		time.wYear,
		time.wMonth,
		time.wDay,
		time.wHour,
		time.wMinute,
		time.wSecond,
		time.wMilliseconds);

	return filename;
}

void FlightTakeScreenshot()
{
	std::wstring filename = BuildScreenshotFilename(L"flightscreen");

	PrimarySurface* primarySurface = *(PrimarySurface**)0x00773344;
	DeviceResources* deviceResources = primarySurface->_deviceResources;

	UINT width = deviceResources->_backbufferWidth;
	UINT height = deviceResources->_backbufferHeight;
	char* buffer = new char[width * height * 4];

	deviceResources->RetrieveBackBuffer(buffer, width, height, 4);

	saveScreenshot(filename, buffer, width, height, 4);

	delete[] buffer;
}

int g_callDrawCursor = true;

void ConcourseTakeScreenshot()
{
	bool takeScreenshot = false;

	MSG msg;
	if (PeekMessage(&msg, nullptr, WM_SYSKEYUP, WM_SYSKEYUP, PM_NOREMOVE))
	{
		if (msg.message == WM_SYSKEYUP && msg.wParam == 'O')
		{
			GetMessage(&msg, nullptr, WM_SYSKEYUP, WM_SYSKEYUP);
			takeScreenshot = true;
		}
	}

	if (!takeScreenshot)
	{
		return;
	}

	std::wstring filename = BuildScreenshotFilename(L"frontscreen");

	FrontbufferSurface* frontSurface = *(FrontbufferSurface**)(0x09F60E0 + 0x0F56);
	DeviceResources* deviceResources = frontSurface->_deviceResources;

	UINT width = deviceResources->_backbufferWidth;
	UINT height = deviceResources->_backbufferHeight;
	char* buffer = new char[width * height * 4];

	deviceResources->RetrieveBackBuffer(buffer, width, height, 4);

	saveScreenshot(filename, buffer, width, height, 4);

	delete[] buffer;
}

void DrawCursor()
{
	if (!g_callDrawCursor)
	{
		return;
	}

	// XwaDrawCursor
	((void(*)())0x0055B630)();
}

void PlayVideoClear()
{
	DirectDraw* MainIDirectDraw = *(DirectDraw**)(0x09F60E0 + 0x0F46);

	MainIDirectDraw->_deviceResources->_d3dDeviceContext->ClearRenderTargetView(MainIDirectDraw->_deviceResources->_renderTargetView, MainIDirectDraw->_deviceResources->clearColor);
	MainIDirectDraw->_deviceResources->_d3dDeviceContext->ClearDepthStencilView(MainIDirectDraw->_deviceResources->_depthStencilViewL, D3D11_CLEAR_DEPTH, MainIDirectDraw->_deviceResources->clearDepth, 0);
	// TODO: Handle SteamVR cases
	((void(*)())0x0055EEA0)();
}
