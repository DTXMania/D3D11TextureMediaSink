#pragma once

class DemoPlay : public IMFAsyncCallback
{
public:
	DemoPlay(HWND hWnd);
	~DemoPlay();

	HRESULT Play(LPCTSTR szFile);
	void Dispose();
	void UpdateAndPresent();

	// IUnknown ŽÀ‘•
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv);
	STDMETHODIMP_(ULONG) Release();

	// IMFAsyncCallback ŽÀ‘•
	HRESULT STDMETHODCALLTYPE GetParameters(__RPC__out DWORD *pdwFlags, __RPC__out DWORD *pdwQueue);
	HRESULT STDMETHODCALLTYPE Invoke(__RPC__in_opt IMFAsyncResult *pAsyncResult);

private:
	long m_refCount;
	BOOL m_bInitialized;

	ID3D11Device* m_pD3DDevice;
	ID3D11DeviceContext* m_pD3DDeviceContext;
	IDXGISwapChain* m_pDXGISwapChain;
	IDXGIOutput* m_pDXGIOutput;
	IMFDXGIDeviceManager* m_pDXGIDeviceManager;
	UINT m_DXGIDeviceManagerID;

	std::atomic_bool m_bPresentNow;
	PTP_WORK m_pWork;
	IMFMediaSession* m_pMediaSession;
	IMFMediaSource* m_pMediaSource;
	IMFMediaSink* m_pMediaSink;
	IMFAttributes* m_pMediaSinkAttributes;
	HANDLE m_evReadyOrFailed;
	HRESULT m_hrStatus;

	void Update();
	void Draw();

	HRESULT CreateMediaSession(IMFMediaSession** ppMediaSession);
	HRESULT CreateMediaSourceFromFile(LPCTSTR szFile, IMFMediaSource** ppMediaSource);
	HRESULT CreateTopology(IMFTopology** ppTopology);
	HRESULT AddTopologyNodes(IMFTopology* pTopology, IMFPresentationDescriptor* pPresentationDesc, DWORD index);
	HRESULT CreateSourceNode(IMFPresentationDescriptor* pPresentationDesc, IMFStreamDescriptor* pStreamDesc, IMFTopologyNode** ppSourceNode);
	HRESULT CreateOutputNode(IMFStreamDescriptor* pStreamDesc, IMFTopologyNode** ppOutputNode, GUID* pMajorType);

	void OnTopologyReady(IMFMediaEvent* pMediaEvent);
	void OnSessionStarted(IMFMediaEvent* pMediaEvent);
	void OnSessionPaused(IMFMediaEvent* pMediaEvent);
	void OnSessionStopped(IMFMediaEvent* pMediaEvent);
	void OnSessionClosed(IMFMediaEvent* pMediaEvent);
	void OnPresentationEnded(IMFMediaEvent* pMediaEvent);
	void OnEndRegistarTopologyWorkQueueWithMMCSS(IMFAsyncResult* pAsyncResult);

	static void CALLBACK PresentSwapChainWork(PTP_CALLBACK_INSTANCE pInstance, LPVOID pvParam, PTP_WORK pWork);
	IUnknown* ID_RegistarTopologyWorkQueueWithMMCSS;
};

