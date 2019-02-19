#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	HRESULT TextureMediaSink::CreateInstance(_In_ REFIID iid, _COM_Outptr_ void** ppSink, void* pDXGIDeviceManager, void* pD3D11Device)
	{
		if (ppSink == NULL)
			return E_POINTER;

		*ppSink = NULL;

		HRESULT hr = S_OK;
		TextureMediaSink* pSink = NULL;
		do
		{
			pSink = new TextureMediaSink();
			if (pSink == NULL)
			{
				hr = E_OUTOFMEMORY;
				break;
			}

			if (FAILED(hr = pSink->Initialize()))
				break;

			pSink->_Presenter->SetD3D11(pDXGIDeviceManager, pD3D11Device);

			if (FAILED(hr = pSink->QueryInterface(iid, ppSink)))
				break;

		} while (FALSE);

		SafeRelease(pSink);

		return hr;
	}

	TextureMediaSink::TextureMediaSink()
	{
		this->_参照カウンタ = 1;
	}
	TextureMediaSink::~TextureMediaSink()
	{
	}

	// IUnknown 実装

	ULONG	TextureMediaSink::AddRef()
	{
		return InterlockedIncrement(&this->_参照カウンタ);
	}
	HRESULT TextureMediaSink::QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
	{
		if (!ppv)
			return E_POINTER;

		if (iid == IID_IUnknown)
		{
			*ppv = static_cast<IUnknown*>(static_cast<IMFMediaSink*>(this));
		}
		else if (iid == __uuidof(IMFMediaSink))
		{
			*ppv = static_cast<IMFMediaSink*>(this);
		}
		else if (iid == __uuidof(IMFClockStateSink))
		{
			*ppv = static_cast<IMFClockStateSink*>(this);
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
	ULONG	TextureMediaSink::Release()
	{
		ULONG uCount = InterlockedDecrement(&this->_参照カウンタ);

		if (uCount == 0)
			delete this;

		return uCount;
	}


	// IMFMediaSink 実装

	HRESULT TextureMediaSink::AddStreamSink(DWORD dwStreamSinkIdentifier, __RPC__in_opt IMFMediaType* pMediaType, __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
	{
		return MF_E_STREAMSINKS_FIXED;
	}
	HRESULT TextureMediaSink::GetCharacteristics(__RPC__out DWORD* pdwCharacteristics)
	{
		AutoLock lock(this->_csMediaSink);

		HRESULT hr;

		if (pdwCharacteristics == NULL)
			return E_POINTER;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		*pdwCharacteristics = MEDIASINK_FIXED_STREAMS;	// ストリーム数は固定

		return S_OK;
	}
	HRESULT TextureMediaSink::GetPresentationClock(__RPC__deref_out_opt IMFPresentationClock** ppPresentationClock)
	{
		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (NULL == ppPresentationClock)
			return E_POINTER;

		// クロックを返す。
		*ppPresentationClock = this->_PresentationClock;
		if (NULL != this->_PresentationClock)
			(*ppPresentationClock)->AddRef();

		return S_OK;
	}
	HRESULT TextureMediaSink::GetStreamSinkById(DWORD dwIdentifier, __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
	{
		HRESULT hr;

		if (NULL == ppStreamSink)
			return E_POINTER;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// ストリームシンクを返す。
		if (dwIdentifier != 0)
			return MF_E_INVALIDSTREAMNUMBER;	// 0 以外は不許可

		return this->_StreamSink->QueryInterface(__uuidof(IMFStreamSink), (void**)ppStreamSink);
	}
	HRESULT TextureMediaSink::GetStreamSinkByIndex(DWORD dwIndex, __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
	{
		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// ストリームシンクを返す。
		if (NULL == ppStreamSink)
			return E_POINTER;

		if (0 != dwIndex)
			return MF_E_INVALIDINDEX;

		*ppStreamSink = this->_StreamSink;
		if (NULL != this->_StreamSink)
			(*ppStreamSink)->AddRef();

		return S_OK;
	}
	HRESULT TextureMediaSink::GetStreamSinkCount(__RPC__out DWORD* pcStreamSinkCount)
	{
		HRESULT hr;

		if (NULL == pcStreamSinkCount)
			return E_POINTER;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// ストリームシンク数を返す。
		*pcStreamSinkCount = 1;		// 1個だけサポート。（固定）

		return S_OK;
	}
	HRESULT TextureMediaSink::RemoveStreamSink(DWORD dwStreamSinkIdentifier)
	{
		return MF_E_STREAMSINKS_FIXED;
	}
	HRESULT TextureMediaSink::SetPresentationClock(__RPC__in_opt IMFPresentationClock* pPresentationClock)
	{
		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// 設定済みのクロックと同じ？
		if (this->_PresentationClock == pPresentationClock)
			return S_OK;

		// 既にクロックを持っているなら、その状態通知を外す。
		if (NULL != this->_PresentationClock)
		{
			if (FAILED(hr = this->_PresentationClock->RemoveClockStateSink(this)))
				return hr;

			SafeRelease(this->_PresentationClock);
		}

		// 新しいクロックに、状態通知を登録する。
		if (NULL != pPresentationClock)
		{
			if (FAILED(hr = pPresentationClock->AddClockStateSink(this)))
				return hr;

			pPresentationClock->AddRef();
		}

		// 新しいクロック（またはnull）を保存する。
		this->_PresentationClock = pPresentationClock;

		return S_OK;
	}
	HRESULT TextureMediaSink::Shutdown()
	{
		if (!this->_Shutdown済み)
		{
			this->_Shutdown済み = TRUE;

			if (NULL != this->_PresentationClock)
				this->_PresentationClock->RemoveClockStateSink(this);
			SafeRelease(this->_PresentationClock);

			this->_Scheduler = NULL;

			this->_StreamSink->Shutdown();
			this->_StreamSink = NULL;

			this->_Presenter->Shutdown();
			this->_Presenter = NULL;
		}

		return MF_E_SHUTDOWN;
	}

	// IMFClockStateSink 実装

	HRESULT TextureMediaSink::OnClockPause(MFTIME hnsSystemTime)
	{
		AutoLock lock(this->_csMediaSink);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// ストリームシンクへ移譲。
		return this->_StreamSink->Pause();
	}
	HRESULT TextureMediaSink::OnClockRestart(MFTIME hnsSystemTime)
	{
		AutoLock lock(this->_csMediaSink);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = CheckShutdown()))
			return hr;

		// ストリームシンクに移譲。
		return this->_StreamSink->Restart();
	}
	HRESULT TextureMediaSink::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
	{
		HRESULT hr = S_OK;

		// スケジューラに新しいレートを設定。
		if (NULL != this->_Scheduler)
			hr = this->_Scheduler->SetClockRate(flRate);

		return hr;
	}
	HRESULT TextureMediaSink::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
	{
		AutoLock lock(this->_csMediaSink);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = CheckShutdown()))
			return hr;

		// Check if the clock is already active (not stopped).
		// And if the clock position changes while the clock is active, it
		// is a seek request. We need to flush all pending samples.
		if (this->_StreamSink->IsActive() && llClockStartOffset != PRESENTATION_CURRENT_POSITION)
		{
			// This call blocks until the scheduler threads discards all scheduled samples.
			if (FAILED(hr = this->_StreamSink->Flush()))
				return hr;
		}
		else
		{
			// スケジューラを開始する。
			if (NULL != this->_Scheduler)
				if (FAILED(hr = this->_Scheduler->Start(this->_PresentationClock)))
					return hr;
		}

		// ストリームシンクを開始する。
		return this->_StreamSink->Start(llClockStartOffset, this->_PresentationClock);
	}
	HRESULT TextureMediaSink::OnClockStop(MFTIME hnsSystemTime)
	{
		AutoLock lock(this->_csMediaSink);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		// ストリームシンクを停止。
		if (FAILED(hr = this->_StreamSink->Stop()))
			return hr;

		// スケジューラを停止。
		if (NULL != this->_Scheduler)
			hr = this->_Scheduler->Stop();

		return S_OK;
	}

	// IMFAttributes 実装

	HRESULT TextureMediaSink::GetUnknown(__RPC__in REFGUID guidKey, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppv)
	{
		if (guidKey == TMS_SAMPLE)
		{
			if (riid == IID_IMFSample)
			{
				IMFSample* pSample = nullptr;
				this->_StreamSink->LockPresentedSample(&pSample);	// この中で pSample->AddRef される
				*ppv = pSample;
				return S_OK;
			}
			return E_NOINTERFACE;
		}
		return E_INVALIDARG;
	}
	HRESULT TextureMediaSink::SetUnknown(__RPC__in REFGUID guidKey, __RPC__in_opt IUnknown* pUnknown)
	{
		if (guidKey == TMS_SAMPLE)
		{
			this->_StreamSink->UnlockPresentedSample();
			return S_OK;
		}
		return E_INVALIDARG;
	}


	// private

	HRESULT TextureMediaSink::Initialize()
	{
		this->_Shutdown済み = FALSE;
		this->_csMediaSink = new CriticalSection();
		this->_csStreamSinkAndScheduler = new CriticalSection();

		this->_PresentationClock = NULL;
		this->_Scheduler = new Scheduler(this->_csStreamSinkAndScheduler);
		this->_Presenter = new Presenter();
		this->_StreamSink = new StreamSink(this, this->_csStreamSinkAndScheduler, this->_Scheduler, this->_Presenter);

		// スケジューラのコールバックに、CStreamSink を登録する。
		this->_Scheduler->SetCallback(static_cast<SchedulerCallback*>(this->_StreamSink));

		return S_OK;
	}
	HRESULT TextureMediaSink::CheckShutdown() const
	{
		return (this->_Shutdown済み) ? MF_E_SHUTDOWN : S_OK;
	}
}
