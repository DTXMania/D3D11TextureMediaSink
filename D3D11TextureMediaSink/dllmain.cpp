#include "stdafx.h"

STDAPI CreateD3D11TextureMediaSink(REFIID riid, void** ppvObject, void* pDXGIDeviceManager, void* pD3D11Device)
{
	return D3D11TextureMediaSink::TextureMediaSink::CreateInstance(riid, ppvObject, pDXGIDeviceManager, pD3D11Device);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
