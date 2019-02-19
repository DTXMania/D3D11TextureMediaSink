#pragma once

namespace D3D11TextureMediaSink
{
	class TextureMediaSink :
		public MFAttributesImpl<IMFAttributes>,
		public IMFMediaSink,
		public IMFClockStateSink
	{
	public:
		static HRESULT CreateInstance(_In_ REFIID iid, _COM_Outptr_ void** ppSink, void* pDXGIDeviceManager, void* pD3D11Device);

		TextureMediaSink();
		~TextureMediaSink();

		void LockPresentedSample(IMFSample** ppSample);
		void UnlockPresentedSample(IMFSample* pSample);

		// IUnknown 宣言

		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv);
		STDMETHODIMP_(ULONG) Release();

		// IMFMediaSink 宣言

		STDMETHODIMP AddStreamSink(DWORD dwStreamSinkIdentifier, __RPC__in_opt IMFMediaType* pMediaType, __RPC__deref_out_opt IMFStreamSink** ppStreamSink);
		STDMETHODIMP GetCharacteristics(__RPC__out DWORD* pdwCharacteristics);
		STDMETHODIMP GetPresentationClock(__RPC__deref_out_opt IMFPresentationClock** ppPresentationClock);
		STDMETHODIMP GetStreamSinkById(DWORD dwIdentifier, __RPC__deref_out_opt IMFStreamSink** ppStreamSink);
		STDMETHODIMP GetStreamSinkByIndex(DWORD dwIndex, __RPC__deref_out_opt IMFStreamSink** ppStreamSink);
		STDMETHODIMP GetStreamSinkCount(__RPC__out DWORD* pcStreamSinkCount);
		STDMETHODIMP RemoveStreamSink(DWORD dwStreamSinkIdentifier);
		STDMETHODIMP SetPresentationClock(__RPC__in_opt IMFPresentationClock* pPresentationClock);
		STDMETHODIMP Shutdown();

		// IMFClockStateSink 宣言

		STDMETHODIMP OnClockPause(MFTIME hnsSystemTime);
		STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime);
		STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate);
		STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
		STDMETHODIMP OnClockStop(MFTIME hnsSystemTime);

		// IMFAttributes 宣言
		STDMETHODIMP GetUnknown(__RPC__in REFGUID guidKey, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppv);
		STDMETHODIMP SetUnknown(__RPC__in REFGUID guidKey, __RPC__in_opt IUnknown* pUnknown);

	private:
		long _参照カウンタ;
		BOOL _Shutdown済み = false;
		IMFPresentationClock* _PresentationClock = NULL;
		StreamSink* _StreamSink = NULL;
		Scheduler* _Scheduler = NULL;
		Presenter* _Presenter = NULL;

		CriticalSection* _csMediaSink;				// MediaSink 内用
		CriticalSection* _csStreamSinkAndScheduler; // StreamSink と Scheduler 用

		HRESULT Initialize();
		HRESULT CheckShutdown() const;
	};
}
