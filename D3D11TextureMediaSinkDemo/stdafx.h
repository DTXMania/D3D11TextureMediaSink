#pragma once

#include "targetver.h"

//#define WIN32_LEAN_AND_MEAN		... comment out for GetOpenFileName

#include <initguid.h>
#include <Windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <mfapi.h>
#include <mfidl.h>
#include <Mferror.h>
#include <d3d11.h>
#include <atomic>

template <class T> inline void SafeRelease(T*& pT)
{
	if (pT != nullptr)
	{
		pT->Release();
		pT = nullptr;
	}
}

#ifdef _DEBUG
#   define _OutputDebugString( str, ... ) \
      { \
        TCHAR buf[256]; \
        _stprintf_s( buf, 256, str, __VA_ARGS__ ); \
        OutputDebugString( buf ); \
      }
#else
#    define _OutputDebugString( str, ... ) // 空実装
#endif

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "D3D11TextureMediaSink.lib")
