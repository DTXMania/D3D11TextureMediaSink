#pragma once

namespace D3D11TextureMediaSink
{
	class StreamSink :
		public MFAttributesImpl<IMFAttributes>,
		public IMFStreamSink,
		public IMFMediaTypeHandler,
		public IMFGetService,
		public SchedulerCallback
	{
	public:
		static GUID const* const s_pVideoFormats[];
		static const DWORD s_dwNumVideoFormats;
		static const MFRatio s_DefaultFrameRate;
		static const struct FormatEntry
		{
			GUID            Subtype;
			DXGI_FORMAT     DXGIFormat;
		} s_DXGIFormatMapping[];

		StreamSink(IMFMediaSink* pParentMediaSink, CriticalSection* critsec, Scheduler* pScheduler, Presenter* pPresenter);
		~StreamSink();

		inline BOOL IsActive() const // IsActive: The "active" state is started or paused.
		{
			return ((this->_State == State_Started) || (this->_State == State_Paused));
		}

		HRESULT Start(MFTIME start, IMFPresentationClock* pClock);
		HRESULT Pause();
		HRESULT Restart();
		HRESULT Stop();
		HRESULT Shutdown();

		void LockPresentedSample(IMFSample** ppSample);
		void UnlockPresentedSample();

		// IUnknown 宣言
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv);
		STDMETHODIMP_(ULONG) Release();

		// IMFStreamSink 宣言
		STDMETHODIMP Flush();
		STDMETHODIMP GetIdentifier(__RPC__out DWORD* pdwIdentifier);
		STDMETHODIMP GetMediaSink(__RPC__deref_out_opt IMFMediaSink** ppMediaSink);
		STDMETHODIMP GetMediaTypeHandler(__RPC__deref_out_opt IMFMediaTypeHandler** ppHandler);
		STDMETHODIMP PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, __RPC__in const PROPVARIANT* pvarMarkerValue, __RPC__in const PROPVARIANT* pvarContextValue);
		STDMETHODIMP ProcessSample(__RPC__in_opt IMFSample* pSample);

		// IMFMediaEventGenerator (in IMFStreamSink) 宣言
		STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState);
		STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, _Out_ IMFMediaEvent** ppEvent);
		STDMETHODIMP GetEvent(DWORD dwFlags, __RPC__deref_out_opt IMFMediaEvent** ppEvent);
		STDMETHODIMP QueueEvent(MediaEventType met, __RPC__in REFGUID guidExtendedType, HRESULT hrStatus, __RPC__in_opt const PROPVARIANT* pvValue);

		// IMFMediaTypeHandler 宣言
		STDMETHODIMP GetCurrentMediaType(_Outptr_ IMFMediaType** ppMediaType);
		STDMETHODIMP GetMajorType(__RPC__out GUID* pguidMajorType);
		STDMETHODIMP GetMediaTypeByIndex(DWORD dwIndex, _Outptr_ IMFMediaType** ppType);
		STDMETHODIMP GetMediaTypeCount(__RPC__out DWORD* pdwTypeCount);
		STDMETHODIMP IsMediaTypeSupported(IMFMediaType* pMediaType, _Outptr_opt_result_maybenull_ IMFMediaType** ppMediaType);
		STDMETHODIMP SetCurrentMediaType(IMFMediaType* pMediaType);

		// IMFGetService 宣言
		STDMETHODIMP GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject);

		// SchedulerCallback 宣言
		HRESULT PresentFrame(IMFSample* pSample);

	private:
		// State enum: Defines the current state of the stream.
		enum State
		{
			State_TypeNotSet = 0,   // No media type is set
			State_Ready,            // Media type is set, Start has never been called.
			State_Started,
			State_Paused,
			State_Stopped,

			State_Count             // Number of states
		};
		// StreamOperation: Defines various operations that can be performed on the stream.
		enum StreamOperation
		{
			OpSetMediaType = 0,
			OpStart,
			OpRestart,
			OpPause,
			OpStop,
			OpProcessSample,
			OpPlaceMarker,

			Op_Count                // Number of operations
		};
		enum ConsumeState
		{
			DropFrames = 0,
			ProcessFrames
		};

		// AsyncOperation class:
		// Used to queue asynchronous operations. When we call MFPutWorkItem, we use this
		// object for the callback state (pState). Then, when the callback is invoked,
		// we can use the object to determine which asynchronous operation to perform.
		// Optional data can include a sample (IMFSample) or a marker.
		// When ProcessSample is called, we use it to queue a sample. When ProcessMarker is
		// called, we use it to queue a marker. This way, samples and markers can live in
		// the same queue. We need this because the sink has to serialize marker events
		// with sample processing.
		class AsyncOperation : public IUnknown
		{
		public:
			AsyncOperation(StreamOperation op);

			// IUnknown methods.
			STDMETHODIMP_(ULONG) AddRef();
			STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv);
			STDMETHODIMP_(ULONG) Release();

			// Structure data.
			StreamOperation m_op;   // The operation to perform.
			PROPVARIANT m_varDataWM;   // Optional data, only used for water mark

		private:
			virtual ~AsyncOperation();
			long m_nRefCount;
		};

		static BOOL ValidStateMatrix[State_Count][Op_Count]; // Defines a look-up table that says which operations are valid from which states.

		long _参照カウンタ;
		BOOL _Shutdown済み = false;
		CriticalSection* _csStreamSinkAndScheduler;
		CriticalSection* _csPresentedSample;

		State _State = State_TypeNotSet;
		IMFMediaSink* _ParentMediaSink = NULL;
		Presenter* _Presenter = NULL;
		Scheduler* _Scheduler = NULL;
		IMFMediaType* _CurrentType = NULL;
		IMFMediaEventQueue* _EventQueue = NULL;
		ThreadSafeComPtrQueue<IUnknown>* _加工前キュー;          // Queue to hold samples and markers. Applies to: ProcessSample, PlaceMarker
		DWORD                       m_WorkQueueId;                  // ID of the work queue for asynchronous operations.
		AsyncCallback<StreamSink> _WorkQueueCB;                  // Callback for the work queue.
		ConsumeState _ConsumeData = ConsumeState::ProcessFrames;                  // Flag to indicate process or drop frames
		DXGI_FORMAT _dxgiFormat = DXGI_FORMAT_UNKNOWN;
		UINT32 _InterlaceMode = MFVideoInterlace_Progressive;
		BOOL _WaitingForOnClockStart = FALSE;
		MFTIME  _StartTime = 0;                  // Presentation time when the clock started.
		DWORD _OutstandingSampleRequests = 0;   // Outstanding reuqests for samples.
		IMFPresentationClock* _PresentationClock = NULL;
		IMFSample* _PresentedSample = NULL;

		HRESULT CheckShutdown() const;
		HRESULT GetFrameRate(IMFMediaType* pType, MFRatio* pRatio);
		HRESULT QueueAsyncOperation(StreamOperation op);
		HRESULT OnDispatchWorkItem(IMFAsyncResult* pAsyncResult);
		HRESULT ProcessSamplesFromQueue(ConsumeState bConsumeData);
		HRESULT ValidateOperation(StreamOperation op);
		HRESULT DispatchProcessSample(AsyncOperation *pOp);
		HRESULT RequestSamples();
		BOOL    NeedMoreSamples();
	};
}
