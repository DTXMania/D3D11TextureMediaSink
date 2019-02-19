#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	// static

	// 優先ビデオフォーマットのリスト
	GUID const* const StreamSink::s_pVideoFormats[] =
	{
		&MFVideoFormat_NV12,
		//&MFVideoFormat_IYUV,
		&MFVideoFormat_YUY2,
		//&MFVideoFormat_YV12,
		&MFVideoFormat_RGB32,
		&MFVideoFormat_ARGB32,
		//&MFVideoFormat_RGB24,
		//&MFVideoFormat_RGB555,
		//&MFVideoFormat_RGB565,
		//&MFVideoFormat_RGB8,
		&MFVideoFormat_AYUV,
		//&MFVideoFormat_UYVY,
		//&MFVideoFormat_YVYU,
		//&MFVideoFormat_YVU9,
		//&MFVideoFormat_v216,
		//&MFVideoFormat_v410,
		//&MFVideoFormat_I420,
		&MFVideoFormat_NV11,
		&MFVideoFormat_420O
	};
	// 優先ビデオフォーマットのリストの要素数
	const DWORD StreamSink::s_dwNumVideoFormats = sizeof(StreamSink::s_pVideoFormats) / sizeof(StreamSink::s_pVideoFormats[0]);

	// ビデオフォーマットとDXGIフォーマットの対応表
	const StreamSink::FormatEntry StreamSink::s_DXGIFormatMapping[] =
	{
		{ MFVideoFormat_RGB32, DXGI_FORMAT_B8G8R8X8_UNORM },
		{ MFVideoFormat_ARGB32, DXGI_FORMAT_R8G8B8A8_UNORM },
		{ MFVideoFormat_AYUV, DXGI_FORMAT_AYUV },
		{ MFVideoFormat_YUY2, DXGI_FORMAT_YUY2 },
		{ MFVideoFormat_NV12, DXGI_FORMAT_NV12 },
		{ MFVideoFormat_NV11, DXGI_FORMAT_NV11 },
		{ MFVideoFormat_AI44, DXGI_FORMAT_AI44 },
		{ MFVideoFormat_P010, DXGI_FORMAT_P010 },
		{ MFVideoFormat_P016, DXGI_FORMAT_P016 },
		{ MFVideoFormat_Y210, DXGI_FORMAT_Y210 },
		{ MFVideoFormat_Y216, DXGI_FORMAT_Y216 },
		{ MFVideoFormat_Y410, DXGI_FORMAT_Y410 },
		{ MFVideoFormat_Y416, DXGI_FORMAT_Y416 },
		{ MFVideoFormat_420O, DXGI_FORMAT_420_OPAQUE },
		{ GUID_NULL, DXGI_FORMAT_UNKNOWN }
	};

	// 操作判定マップ
	BOOL StreamSink::ValidStateMatrix[StreamSink::State_Count][StreamSink::Op_Count] =
	{
		// States:    Operations:
		//            SetType   Start     Restart   Pause     Stop      Sample    Marker
		/* NotSet */  TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,

		/* Ready */   TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE,

		/* Start */   TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE,

		/* Pause */   TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,

		/* Stop */    TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE

		// Note about states:
		// 1. OnClockRestart should only be called from paused state.
		// 2. While paused, the sink accepts samples but does not process them.
	};

	const MFRatio StreamSink::s_DefaultFrameRate = { 30, 1 };

	// Control how we batch work from the decoder.
	// On receiving a sample we request another one if the number on the queue is
	// less than the hi water threshold.
	// When displaying samples (removing them from the sample queue) we request
	// another one if the number of falls below the lo water threshold
	//
#define SAMPLE_QUEUE_HIWATER_THRESHOLD 3
	// maximum # of past reference frames required for deinterlacing
#define MAX_PAST_FRAMES         3


	// method

	StreamSink::StreamSink(IMFMediaSink* pParentMediaSink, CriticalSection* critsec, Scheduler* pScheduler, Presenter* pPresenter) :
		_WorkQueueCB(this, &StreamSink::OnDispatchWorkItem)
	{
		this->Initialize();

		this->_参照カウンタ = 1;
		this->_Shutdown済み = FALSE;
		this->_加工前キュー = new ThreadSafeComPtrQueue<IUnknown>();
		this->_ParentMediaSink = pParentMediaSink;
		this->_csStreamSinkAndScheduler = critsec;
		this->_csPresentedSample = new CriticalSection();
		this->_Scheduler = pScheduler;
		this->_Presenter = pPresenter;

		::MFAllocateWorkQueueEx(MF_STANDARD_WORKQUEUE, &this->m_WorkQueueId);

		// イベントキューを生成。
		::MFCreateEventQueue(&this->_EventQueue);
	}
	StreamSink::~StreamSink()
	{
	}

	HRESULT StreamSink::Start(MFTIME start, IMFPresentationClock* pClock)
	{
		//OutputDebugString(L"StreamSink::Start\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr = S_OK;

		do
		{
			// 操作可能？
			if (FAILED(hr = this->ValidateOperation(OpStart)))
				break;

			if (start != PRESENTATION_CURRENT_POSITION)
			{
				// We're starting from a "new" position
				this->_StartTime = start;        // Cache the start time.
			}

			// プレゼンテーションクロックを更新。
			SafeRelease(this->_PresentationClock);
			this->_PresentationClock = pClock;
			if (NULL != this->_PresentationClock)
				this->_PresentationClock->AddRef();

			// 非同期操作 Start を実行。
			this->_State = State_Started;
			hr = this->QueueAsyncOperation(OpStart);

		} while (FALSE);

		this->_WaitingForOnClockStart = FALSE;

		return hr;
	}
	HRESULT StreamSink::Pause()
	{
		//OutputDebugString(L"StreamSink::Pause\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		// 操作許可？
		if (FAILED(hr = ValidateOperation(OpPause)))
			return hr;

		// 同期操作 Pause を実行
		this->_State = State_Paused;
		return this->QueueAsyncOperation(OpPause);
	}
	HRESULT StreamSink::Restart()
	{
		//OutputDebugString(L"StreamSink::Restart\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		// 操作可能？
		if (FAILED(hr = ValidateOperation(OpRestart)))
			return hr;

		// 非同期操作 Restart を実行
		this->_State = State_Started;
		return this->QueueAsyncOperation(OpRestart);
	}
	HRESULT StreamSink::Stop()
	{
		//OutputDebugString(L"StreamSink::Stop\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		// 操作可能？
		if (FAILED(hr = ValidateOperation(OpStop)))
			return hr;

		// プレゼンテーションクロックを解放。
		SafeRelease(this->_PresentationClock);

		// 非同期操作 Stop を実行。
		this->_State = State_Stopped;
		return this->QueueAsyncOperation(OpStop);
	}
	HRESULT StreamSink::Shutdown()
	{
		//OutputDebugString(L"StreamSink::Shutdown\n");

		AutoLock(this->_csStreamSinkAndScheduler);

		this->_EventQueue->Shutdown();
		SafeRelease(this->_EventQueue);

		::MFUnlockWorkQueue(this->m_WorkQueueId);

		this->_加工前キュー->Clear();

		SafeRelease(this->_PresentationClock);

		this->_ParentMediaSink = NULL;
		this->_Presenter = NULL;
		this->_Scheduler = NULL;
		this->_CurrentType = NULL;

		return S_OK;
	}

	void StreamSink::LockPresentedSample(IMFSample** ppSample)
	{
		this->_csPresentedSample->Lock();
		*ppSample = this->_PresentedSample;
		if (*ppSample != NULL)
			(*ppSample)->AddRef();
	}
	void StreamSink::UnlockPresentedSample()
	{
		this->_csPresentedSample->Unlock();
	}

	// IUnknown 実装

	ULONG	StreamSink::AddRef()
	{
		return InterlockedIncrement(&this->_参照カウンタ);
	}
	HRESULT StreamSink::QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
	{
		if (NULL == ppv)
			return E_POINTER;

		if (iid == IID_IUnknown)
		{
			*ppv = static_cast<IUnknown*>(static_cast<IMFStreamSink*>(this));
		}
		else if (iid == __uuidof(IMFStreamSink))
		{
			*ppv = static_cast<IMFStreamSink*>(this);
		}
		else if (iid == __uuidof(IMFMediaEventGenerator))
		{
			*ppv = static_cast<IMFMediaEventGenerator*>(this);
		}
		else if (iid == __uuidof(IMFMediaTypeHandler))
		{
			*ppv = static_cast<IMFMediaTypeHandler*>(this);
		}
		else if (iid == __uuidof(IMFGetService))
		{
			*ppv = static_cast<IMFGetService*>(this);
		}
		else if (iid == __uuidof(IMFAttributes))
		{
			*ppv = static_cast<IMFAttributes*>(this);
		}
		else
		{
			*ppv = NULL;
			return E_NOINTERFACE;
		}

		this->AddRef();

		return S_OK;
	}
	ULONG	StreamSink::Release()
	{
		ULONG uCount = InterlockedDecrement(&this->_参照カウンタ);

		if (uCount == 0)
			delete this;

		return uCount;
	}

	// IMFStreamSink 実装

	HRESULT	StreamSink::Flush()
	{
		//OutputDebugString(L"StreamSink::Flush\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		do
		{
			if (FAILED(hr = this->CheckShutdown()))
				break;

			this->_ConsumeData = DropFrames;
			// Note: Even though we are flushing data, we still need to send
			// any marker events that were queued.
			if (FAILED(hr = this->ProcessSamplesFromQueue(this->_ConsumeData)))
				break;

			hr = this->_Scheduler->Flush();	// This call blocks until the scheduler threads discards all scheduled samples.
			hr = this->_Presenter->Flush();

		} while (FALSE);

		this->_ConsumeData = ProcessFrames;

		return hr;
	}
	HRESULT StreamSink::GetIdentifier(__RPC__out DWORD* pdwIdentifier)
	{
		//OutputDebugString(L"StreamSink::GetIdentifier\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		if (pdwIdentifier == NULL)
			return E_POINTER;

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// ストリームIDを返す。
		*pdwIdentifier = 0;	// 固定

		return hr;
	}
	HRESULT StreamSink::GetMediaSink(__RPC__deref_out_opt IMFMediaSink** ppMediaSink)
	{
		//OutputDebugString(L"StreamSink::GetMediaSink\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		if (ppMediaSink == NULL)
			return E_POINTER;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		*ppMediaSink = this->_ParentMediaSink;
		(*ppMediaSink)->AddRef();

		return S_OK;
	}
	HRESULT StreamSink::GetMediaTypeHandler(__RPC__deref_out_opt IMFMediaTypeHandler** ppHandler)
	{
		//OutputDebugString(L"StreamSink::GetMediaTypeHandler\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		if (ppHandler == NULL)
			return E_POINTER;

		HRESULT hr;

		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// このストリームシンクは自身のタイプハンドラとしても働くので、自身に QueryInterface する。
		return this->QueryInterface(IID_IMFMediaTypeHandler, (void**)ppHandler);
	}
	HRESULT StreamSink::PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, __RPC__in const PROPVARIANT* pvarMarkerValue, __RPC__in const PROPVARIANT* pvarContextValue)
	{
		//OutputDebugString(L"StreamSink::PlaceMarker\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr = S_OK;
		IMarker* pMarker = NULL;

		do
		{
			// シャットダウン済み？
			if (FAILED(hr = this->CheckShutdown()))
				break;

			// 操作可能？
			if (FAILED(hr = this->ValidateOperation(OpPlaceMarker)))
				break;

			// マーカーを作成し、加工前キューに格納する。
			if (FAILED(hr = Marker::Create(eMarkerType, pvarMarkerValue, pvarContextValue, &pMarker)))
				break;
			if (FAILED(hr = this->_加工前キュー->Queue(pMarker)))
				break;

			// 一時停止中ではないなら、非同期操作 sample/marker を実行する。
			if (this->_State != State_Paused)
			{
				if (FAILED(hr = this->QueueAsyncOperation(OpPlaceMarker)))
					break;
			}

		} while (FALSE);

		SafeRelease(pMarker);

		return hr;
	}
	HRESULT StreamSink::ProcessSample(__RPC__in_opt IMFSample* pSample)
	{
		HRESULT hr = S_OK;

		//OutputDebugString(L"StreamSink::ProcessSample\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		if (NULL == pSample)
			return E_POINTER;

		if (0 == this->_OutstandingSampleRequests)
			return MF_E_INVALIDREQUEST;	// 誰もサンプルを要求していない


		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		this->_OutstandingSampleRequests--;

		// 操作可能？
		if (!this->_WaitingForOnClockStart)
		{
			if (FAILED(hr = this->ValidateOperation(OpProcessSample)))
				return hr;
		}

		// サンプルを加工前キューに追加する。
		if (FAILED(hr = this->_加工前キュー->Queue(pSample)))
			return hr;

		// 停止/一時停止中でない場合、非同期操作 processSample を実行する。
		if (this->_State != State_Paused && this->_State != State_Stopped)
		{
			if (FAILED(hr = this->QueueAsyncOperation(OpProcessSample)))
				return hr;
		}

		return S_OK;
	}

	// IMFMediaEventGenerator (in IMFStreamSink) 実装

	HRESULT StreamSink::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState)
	{
		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// イベントキューに移譲。
		return this->_EventQueue->BeginGetEvent(pCallback, punkState);
	}
	HRESULT StreamSink::EndGetEvent(IMFAsyncResult* pResult, _Out_ IMFMediaEvent** ppEvent)
	{
		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// イベントキューに移譲。
		return this->_EventQueue->EndGetEvent(pResult, ppEvent);
	}
	HRESULT StreamSink::GetEvent(DWORD dwFlags, __RPC__deref_out_opt IMFMediaEvent** ppEvent)
	{
		// NOTE:
		// GetEvent can block indefinitely, so we don't hold the lock.
		// This requires some juggling with the event queue pointer.

		HRESULT hr;
		IMFMediaEventQueue* pQueue = NULL;

		do
		{
			// scope for lock
			{
				AutoLock lock(this->_csStreamSinkAndScheduler);

				// シャットダウン済み？
				if (FAILED(hr = this->CheckShutdown()))
					break;

				// Get the pointer to the event queue.
				pQueue = this->_EventQueue;
				pQueue->AddRef();
			}

			// Now get the event.
			hr = pQueue->GetEvent(dwFlags, ppEvent);

		} while (FALSE);

		SafeRelease(pQueue);

		return hr;
	}
	HRESULT StreamSink::QueueEvent(MediaEventType met, __RPC__in REFGUID guidExtendedType, HRESULT hrStatus, __RPC__in_opt const PROPVARIANT* pvValue)
	{
		//OutputDebugString(L"StreamSink::QueueEvent\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// イベントキューに移譲。
		return this->_EventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
	}

	// IMFMediaTypeHandler 実装

	HRESULT StreamSink::GetCurrentMediaType(_Outptr_ IMFMediaType** ppMediaType)
	{
		//OutputDebugString(L"StreamSink::GetCurrentMediaType\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		if (ppMediaType == NULL)
			return E_POINTER;

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (NULL == this->_CurrentType)
			return MF_E_NOT_INITIALIZED;

		*ppMediaType = this->_CurrentType;
		(*ppMediaType)->AddRef();

		return S_OK;
	}
	HRESULT StreamSink::GetMajorType(__RPC__out GUID* pguidMajorType)
	{
		//OutputDebugString(L"StreamSink::GetMajorType\n");

		if (NULL == pguidMajorType)
			return E_POINTER;

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// メディアタイプ未設定？
		if (NULL == this->_CurrentType)
			return MF_E_NOT_INITIALIZED;

		// 現在のメディアタイプのメジャータイプを返す。
		return this->_CurrentType->GetGUID(MF_MT_MAJOR_TYPE, pguidMajorType);
	}
	HRESULT StreamSink::GetMediaTypeByIndex(DWORD dwIndex, _Outptr_ IMFMediaType** ppType)
	{
		//OutputDebugString(L"StreamSink::GetMediaTypeByIndex\n");

		HRESULT hr = S_OK;

		if (NULL == ppType)
			return E_POINTER;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (dwIndex >= s_dwNumVideoFormats)
			return MF_E_NO_MORE_TYPES;	// インデックスが範囲外

		IMFMediaType* pVideoMediaType = NULL;

		// dwIndex 番目の優先ビデオフォーマットに対応するメディアタイプを作成して返す。
		do
		{
			if (FAILED(hr = ::MFCreateMediaType(&pVideoMediaType)))
				break;

			if (FAILED(hr = pVideoMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)))
				break;

			if (FAILED(hr = pVideoMediaType->SetGUID(MF_MT_SUBTYPE, *(s_pVideoFormats[dwIndex]))))
				break;

			pVideoMediaType->AddRef();
			*ppType = pVideoMediaType;

		} while (FALSE);

		SafeRelease(pVideoMediaType);

		return S_OK;
	}
	HRESULT StreamSink::GetMediaTypeCount(__RPC__out DWORD* pdwTypeCount)
	{
		//OutputDebugString(L"StreamSink::GetMediaTypeCount\n");

		HRESULT hr = S_OK;

		if (NULL == pdwTypeCount)
			return E_POINTER;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// 優先ビデオフォーマット数（＝優先メディアタイプ数）を返す。
		*pdwTypeCount = s_dwNumVideoFormats;

		return S_OK;
	}
	HRESULT StreamSink::IsMediaTypeSupported(IMFMediaType* pMediaType, _Outptr_opt_result_maybenull_ IMFMediaType** ppMediaType)
	{
		//OutputDebugString(L"StreamSink::IsMediaTypeSuppored\n");

		HRESULT hr = S_OK;
		GUID subType = GUID_NULL;

		// We don't return any "close match" types.
		if (ppMediaType)
			*ppMediaType = NULL;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (NULL == pMediaType)
			return E_POINTER;

		// 検査対象メディアタイプのサブタイプを取得。
		if (FAILED(hr = pMediaType->GetGUID(MF_MT_SUBTYPE, &subType)))
			return hr;

		// 優先フォーマットリストにある？
		hr = MF_E_INVALIDMEDIATYPE;
		for (DWORD i = 0; i < s_dwNumVideoFormats; i++)
		{
			if (subType == (*s_pVideoFormats[i]))
			{
				hr = S_OK;
				break;
			}
		}
		if (FAILED(hr))
			return hr;		// ない；優先フォーマットが対応していない

		// 対応するDXGIフォーマットを取得。
		DWORD i = 0;
		while (s_DXGIFormatMapping[i].Subtype != GUID_NULL)
		{
			const FormatEntry& e = s_DXGIFormatMapping[i];
			if (e.Subtype == subType)
			{
				this->_dxgiFormat = e.DXGIFormat;
				break;
			}
			i++;
		}

		// プレゼンタで対応している？
		if (FAILED(hr = this->_Presenter->IsSupported(pMediaType, this->_dxgiFormat)))
			return hr;

		return S_OK;
	}
	HRESULT StreamSink::SetCurrentMediaType(IMFMediaType* pMediaType)
	{
		//OutputDebugString(L"StreamSink::SetCurrentMediaType\n");

		if (pMediaType == NULL)
			return E_POINTER;

		HRESULT hr = S_OK;

		AutoLock lock(this->_csStreamSinkAndScheduler);


		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// 操作可能？
		if (FAILED(hr = this->ValidateOperation(OpSetMediaType)))
			return hr;

		// 指定されたメディアタイプはサポートされる？
		if (FAILED(hr = this->IsMediaTypeSupported(pMediaType, NULL)))
			return hr;

		// 現在のメディアタイプを、指定されたメディアタイプに更新。
		SafeRelease(this->_CurrentType);
		this->_CurrentType = pMediaType;
		this->_CurrentType->AddRef();

		// インタレースモードを取得して保存。
		hr = pMediaType->GetUINT32(MF_MT_INTERLACE_MODE, &this->_InterlaceMode);

		// フレームレートをスケジューラに設定する。
		MFRatio fps = { 0, 0 };
		if (SUCCEEDED(this->GetFrameRate(pMediaType, &fps)) && (fps.Numerator != 0) && (fps.Denominator != 0))
		{
			if (MFVideoInterlace_FieldInterleavedUpperFirst == this->_InterlaceMode ||
				MFVideoInterlace_FieldInterleavedLowerFirst == this->_InterlaceMode ||
				MFVideoInterlace_FieldSingleUpper == this->_InterlaceMode ||
				MFVideoInterlace_FieldSingleLower == this->_InterlaceMode ||
				MFVideoInterlace_MixedInterlaceOrProgressive == this->_InterlaceMode)
			{
				fps.Numerator *= 2;
			}
			this->_Scheduler->SetFrameRate(fps);
		}
		else
		{
			// NOTE: The mixer's proposed type might not have a frame rate, in which case
			// we'll use an arbitary default. (Although it's unlikely the video source
			// does not have a frame rate.)
			this->_Scheduler->SetFrameRate(s_DefaultFrameRate);
		}

		// 必要となるサンプル数を、メディアタイプをもとに修正する（プログレッシブ or インタレース）
		if (this->_InterlaceMode == MFVideoInterlace_Progressive)
		{
			// XVP will hold on to 1 sample but that's the same sample we will internally hold on to
			hr = this->SetUINT32(MF_SA_REQUIRED_SAMPLE_COUNT, SAMPLE_QUEUE_HIWATER_THRESHOLD);
		}
		else
		{
			// Assume we will need a maximum of 3 backward reference frames for deinterlacing
			// However, one of the frames is "shared" with SVR
			hr = this->SetUINT32(MF_SA_REQUIRED_SAMPLE_COUNT, SAMPLE_QUEUE_HIWATER_THRESHOLD + MAX_PAST_FRAMES - 1);
		}

		// プレゼンタにメディアタイプを設定する。
		if (SUCCEEDED(hr))
		{
			if (FAILED(hr = this->_Presenter->SetCurrentMediaType(pMediaType)))
				return hr;
		}

		// 開始または一時停止ではないなら、準備ステータスに戻す。
		if (State_Started != this->_State && State_Paused != this->_State)
		{
			this->_State = State_Ready;
		}
		else
		{
			// 開始または一時停止中であれば、フラッシュする。
			//Flush all current samples in the Queue as this is a format change
			hr = this->Flush();
		}

		return hr;
	}

	// IMFGetService 実装

	HRESULT StreamSink::GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject)
	{
		//OutputDebugString(L"StreamSink::GetService\n");

		HRESULT hr;

		if (guidService == MR_VIDEO_ACCELERATION_SERVICE)
		{
			if (riid == __uuidof(IMFDXGIDeviceManager))
			{
				*ppvObject = (void*) static_cast<IUnknown*>(this->_Presenter->GetDXGIDeviceManager());
				return S_OK;
			}
			else
			{
				return E_NOINTERFACE;
			}
		}
		else
		{
			return MF_E_UNSUPPORTED_SERVICE;
		}

		return hr;
	}

	// SchedulerCallback 実装

	HRESULT StreamSink::PresentFrame(IMFSample* pSample)
	{
		//_OutputDebugString(L"StreamSink::PresentFrame\n");

		AutoLock lock(this->_csPresentedSample);

		HRESULT hr = S_OK;

		// 現在のサンプルを解放する。
		if (NULL != this->_PresentedSample)
			this->_Presenter->ReleaseSample(this->_PresentedSample);

		// 指定されたサンプルの SAMPLE_STATE を PRESENT に更新。
		if (FAILED(hr = pSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_PRESENT)))
			return hr;



		//LONGLONG time;
		//pSample->GetSampleTime(&time);
		//_OutputDebugString(_T("StreamSink::PresentFrame; %lld\n"), time);


		// 入れ替え。
		this->_PresentedSample = pSample;

		return S_OK;
	}


	// private

	HRESULT StreamSink::CheckShutdown() const
	{
		return (this->_Shutdown済み) ? MF_E_SHUTDOWN : S_OK;
	}
	HRESULT StreamSink::GetFrameRate(IMFMediaType* pType, MFRatio* pRatio)
	{
		if (NULL == pRatio)
			return E_POINTER;

		// 指定されたメディアタイプのフレームレートを取得する。
		return ::MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, (UINT32*)&pRatio->Numerator, (UINT32*)&pRatio->Denominator);
	}
	HRESULT StreamSink::ValidateOperation(StreamOperation op)
	{
		HRESULT hr = S_OK;

		BOOL bTransitionAllowed = ValidStateMatrix[this->_State][op];

		if (bTransitionAllowed)
		{
			return S_OK;
		}
		else
		{
			return MF_E_INVALIDREQUEST;
		}
	}

	HRESULT StreamSink::QueueAsyncOperation(StreamOperation op)
	{
		//OutputDebugString(L"StreamSink::QueueAsyncOperation\n");

		HRESULT hr = S_OK;

		AsyncOperation* pOp = new AsyncOperation(op);
		do
		{
			if (NULL == pOp)
			{
				hr = E_OUTOFMEMORY;
				break;
			}

			// ワークキューに非同期アイテムを追加。
			if (FAILED(hr = ::MFPutWorkItem(this->m_WorkQueueId, &this->_WorkQueueCB, pOp)))
				break;

		} while (FALSE);

		SafeRelease(pOp);

		return hr;
	}
	HRESULT StreamSink::OnDispatchWorkItem(IMFAsyncResult* pAsyncResult)
	{
		//OutputDebugString(L"StreamSink::OnDispatchWorkItem\n");

		AutoLock lock(this->_csStreamSinkAndScheduler);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;


		IUnknown* pState = NULL;

		if (SUCCEEDED(hr = pAsyncResult->GetState(&pState)))
		{
			AsyncOperation* pOp = (AsyncOperation*)pState;
			StreamOperation op = pOp->m_op;

			switch (op)
			{
			case OpStart:
			case OpRestart:

				//_OutputDebugString(_T("OnStart/OnRestart\n"));

				// クライアントに MEStreamSinkStarted を送信。
				if (FAILED(hr = this->QueueEvent(MEStreamSinkStarted, GUID_NULL, hr, NULL)))
					break;

				// 2つのサンプルを要求することから始まる……
				this->_OutstandingSampleRequests++;
				if (FAILED(hr = this->QueueEvent(MEStreamSinkRequestSample, GUID_NULL, hr, NULL)))
					break;

				// 既にサンプルキューに入ってるかもしれない（一時停止時など）
				if (FAILED(hr = this->ProcessSamplesFromQueue(this->_ConsumeData)))
					break;
				break;

			case OpStop:

				// キューのサンプルを破棄する。
				this->Flush();
				this->_OutstandingSampleRequests = 0;

				// クライアントに MEStreamSinkStopped を送信する。
				if (FAILED(hr = this->QueueEvent(MEStreamSinkStopped, GUID_NULL, hr, NULL)))
					break;
				break;

			case OpPause:

				// クライアントに MFStreamSinkPaused を送信する。
				if (FAILED(hr = this->QueueEvent(MEStreamSinkPaused, GUID_NULL, hr, NULL)))
					break;
				break;

			case OpProcessSample:
			case OpPlaceMarker:

				if (!(this->_WaitingForOnClockStart))
				{
					if (FAILED(hr = this->DispatchProcessSample(pOp)))
						break;
				}
				break;
			}

			SafeRelease(pState);
		}

		return hr;
	}
	HRESULT StreamSink::DispatchProcessSample(AsyncOperation *pOp)
	{
		//OutputDebugString(L"StreamSink::DispatchProcessSample\n");

		HRESULT hr = S_OK;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// 次のサンプルを処理していい？
		if (this->_Presenter->IsReadyNextSample())
		{
			do
			{
				// キュー内のサンプルを処理する。
				if (FAILED(hr = this->ProcessSamplesFromQueue(ProcessFrames)))
					break;

				// 他のサンプルがあるか確認する。
				if (pOp->m_op == OpProcessSample)
				{
					if (FAILED(hr = this->RequestSamples()))
						break;
				}

			} while (FALSE);

			// 非同期操作の途中で何かが失敗したなら、クライアントに MEError を送信する。
			if (FAILED(hr))
				hr = this->QueueEvent(MEError, GUID_NULL, hr, NULL);
		}

		return hr;
	}
	HRESULT StreamSink::ProcessSamplesFromQueue(ConsumeState bConsumeData)
	{
		//OutputDebugString(L"StreamSink::ProcessSamplesFromQueue\n");

		HRESULT hr = S_OK;
		IUnknown* pUnk = nullptr;
		BOOL bProcessMoreSamples = TRUE;
		BOOL bDeviceChanged = FALSE;
		BOOL bProcessAgain = FALSE;

		// キューの中のサンプル／マーカーについて……
		while (this->_加工前キュー->Dequeue(&pUnk) == S_OK)	// 空の場合、Dequeue() は S_FALSE を返す。
		{
			bProcessMoreSamples = TRUE;
			IMarker* pMarker = NULL;
			IMFSample* pSample = NULL;
			IMFSample* pOutSample = NULL;

			do
			{
				// pUnk がマーカー or サンプルのいずれであるかを確認する。
				if ((hr = pUnk->QueryInterface(__uuidof(IMarker), (void**)&pMarker)) == E_NOINTERFACE)
				{
					if (FAILED(hr = pUnk->QueryInterface(IID_IMFSample, (void**)&pSample)))
						break;
				}

				// マーカーとサンプルで処理分岐。

				if (pMarker)
				{
					// (A) マーカーの場合 

					HRESULT hrStatus = S_OK;  // Status code for marker event.

					PROPVARIANT var;
					::PropVariantInit(&var);

					if (this->_ConsumeData == DropFrames)
						hrStatus = E_ABORT;

					do
					{
						if (FAILED(hr = pMarker->GetContext(&var)))
							break;

						// クライアントにマーカーのステータスを通信。
						if (FAILED(hr = this->QueueEvent(MEStreamSinkMarker, GUID_NULL, hrStatus, &var)))
							break;

					} while (FALSE);

					::PropVariantClear(&var);

					hr = S_OK;
					break;
				}
				else
				{
					// (B) サンプルの場合

					if (bConsumeData == ProcessFrames)
					{
						// クロックを確認する。
						LONGLONG clockTime;
						MFTIME systemTime;
						if (FAILED(hr = this->_PresentationClock->GetCorrelatedTime(0, &clockTime, &systemTime)))	// 現在時刻取得（ビデオ処理前）
							break;
						LONGLONG sampleTime;
						if (FAILED(hr = pSample->GetSampleTime(&sampleTime)))	// サンプルの表示時刻取得
							break;
						if (sampleTime < clockTime)
						{
							// クロックに対してサンプルが遅れている場合は、ここでサンプルを破棄（ドロップ）する。
							_OutputDebugString(_T("drop1.[sampleTime:%lld, clockTime:%lld]\n"), sampleTime / 10000, clockTime / 10000);
							break;
						}

						// プレゼンタでサンプルに対するビデオ処理を行い、出力サンプルを得る。
						if (FAILED(hr = this->_Presenter->ProcessFrame(this->_CurrentType, pSample, &this->_InterlaceMode, &bDeviceChanged, &bProcessAgain, &pOutSample)))
							break;

						// デバイスが変わってたらクライアントに通知。
						if (bDeviceChanged)
							if (FAILED(hr = this->QueueEvent(MEStreamSinkDeviceChanged, GUID_NULL, S_OK, NULL)))
								break;

						// 入力サンプルが使われなかったら、キューに戻す。
						if (bProcessAgain)
							if (FAILED(hr = this->_加工前キュー->PutBack(pSample)))
								break;

						if (FAILED(hr = this->_PresentationClock->GetCorrelatedTime(0, &clockTime, &systemTime)))	// もう一度現在時刻取得（ビデオ処理後）
							break;

						if (sampleTime < clockTime)
						{
							// クロックに対してサンプルが遅れている場合は、ここでサンプルを破棄（ドロップ）するその２。
							_OutputDebugString(_T("drop2.[sampleTime:%lld, clockTime:%lld]\n"), sampleTime / 10000, clockTime / 10000);
							break;
						}

						if (NULL != pOutSample)
						{
							bool bPresentNow = (State_Started != this->_State);	// 通常再生時以外はすぐに表示する（スクラブ時、再描画時など）
							hr = this->_Scheduler->ScheduleSample(pOutSample, bPresentNow);
							bProcessMoreSamples = FALSE;

							//_OutputDebugString(_T("scheduled.[sampleTime:%lld, clockTime:%lld]\n"), sampleTime / 10000, clockTime / 10000);
						}
					}
				}
			} while (FALSE);

			SafeRelease(pUnk);
			SafeRelease(pMarker);
			SafeRelease(pSample);
			SafeRelease(pOutSample);

			if (!bProcessMoreSamples)
				break;

		} // while loop

		SafeRelease(pUnk);

		return hr;
	}
	HRESULT StreamSink::RequestSamples()
	{
		//OutputDebugString(L"StreamSink::RequestSamples\n");

		HRESULT hr = S_OK;

		while (this->NeedMoreSamples())
		{
			// シャットダウン済み？
			if (FAILED(hr = this->CheckShutdown()))
				return hr;

			// クライアントに MEStreamSinkRequestSample を送信する。
			this->_OutstandingSampleRequests++;
			if (FAILED(hr = this->QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL)))
				return hr;
		}

		return hr;
	}
	BOOL StreamSink::NeedMoreSamples()
	{
		const DWORD cSamplesInFlight = this->_加工前キュー->GetCount() + this->_OutstandingSampleRequests;

		return (cSamplesInFlight < SAMPLE_QUEUE_HIWATER_THRESHOLD);
	}


	// AcyncOperation クラス実装

	StreamSink::AsyncOperation::AsyncOperation(StreamOperation op)
	{
		this->m_nRefCount = 1;
		this->m_op = op;
	}
	StreamSink::AsyncOperation::~AsyncOperation()
	{
	}

	ULONG StreamSink::AsyncOperation::AddRef()
	{
		return InterlockedIncrement(&this->m_nRefCount);
	}
	HRESULT StreamSink::AsyncOperation::QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
	{
		if (NULL == ppv)
			return E_POINTER;

		if (iid == IID_IUnknown)
		{
			*ppv = static_cast<IUnknown*>(this);
		}
		else
		{
			*ppv = NULL;
			return E_NOINTERFACE;
		}

		this->AddRef();

		return S_OK;
	}
	ULONG StreamSink::AsyncOperation::Release()
	{
		ULONG uCount = InterlockedDecrement(&this->m_nRefCount);

		if (uCount == 0)
			delete this;

		return uCount;
	}

}
