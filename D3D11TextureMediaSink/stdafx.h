// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、
// または、参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーからほとんど使用されていない部分を除外する


#include <initguid.h>
#include <windows.h>
#include <tchar.h>
#include <strmif.h>
#include <strsafe.h>
#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <evr.h>
#include <d3d11.h>
#include <cguid.h>
#include <process.h>
#include <avrt.h>
#include <assert.h>

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


#include "D3D11TextureMediaSink.h"
#include "AsyncCallback.h"
#include "CriticalSection.h"
#include "AutoLock.h"
#include "ComPtrListEx.h"
#include "MFAttributesImpl.h"
#include "PtrList.h"
#include "ThreadSafeComPtrQueue.h"
#include "ThreadSafePtrQueue.h"
#include "IMarker.h"
#include "Marker.h"
#include "SampleAllocator.h"
#include "Scheduler.h"
#include "Presenter.h"
#include "StreamSink.h"
#include "TextureMediaSink.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winmm.lib")
