#include "Runtime.h"
#include "..\Imaging\ImageResource.h"

HINSTANCE g_hInstance = NULL;

COLORREF WINAPI ResolveQuadooColor(COLORREF color)
{
	return (color & 0xff000000) == 0x80000000 ? GetSysColor(static_cast<INT>(color & 0x00ffffff)) : color & 0x00ffffff;
}

BOOL WINAPI DllMain (HINSTANCE instance, DWORD reason, LPVOID)
{
	if(reason == DLL_PROCESS_ATTACH)
	{
		g_hInstance = instance;
		DisableThreadLibraryCalls(instance);
	}
	if(reason == DLL_PROCESS_DETACH)
	{
		QFI::ReleaseCodecTLS();
	}
	return TRUE;
}
