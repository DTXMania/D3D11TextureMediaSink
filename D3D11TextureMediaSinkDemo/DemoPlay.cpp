#include "stdafx.h"
#include "../D3D11TextureMediaSink/D3D11TextureMediaSink.h"
#include "DemoPlay.h"

DemoPlay::DemoPlay(HWND hWnd)
{
	HRESULT hr = S_OK;
	m_refCount = 0;
	m_bInitialized = FALSE;
	m_hrStatus = S_OK;
	m_bPresentNow = false;
	m_pWork = ::CreateThreadpoolWork(DemoPlay::PresentSwapChainWork, this, NULL);
	m_evReadyOrFailed = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	// MediaFoundation のセットアップ
	::MFStartup(MF_VERSION);

	// ID3D11Device と IDXGISwapChain の作成
	D3D_FEATURE_LEVEL featureLevels[] =	{ D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL feature_level;
	RECT clientRect;
	::GetClientRect(hWnd, &clientRect);
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	::ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.BufferDesc.Width = clientRect.right - clientRect.left;
	swapChainDesc.BufferDesc.Height= clientRect.bottom - clientRect.top;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Windowed = TRUE;
	hr = ::D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		featureLevels,
		1,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&m_pDXGISwapChain,
		&m_pD3DDevice,
		&feature_level,
		&m_pD3DDeviceContext);
	if (FAILED(hr))
		return;

	// マルチスレッドモードを ON に設定する。DXVAを使う場合は必須。
	ID3D10Multithread* pD3D10Multithread;
	do
	{
		if (FAILED(hr = m_pD3DDevice->QueryInterface(IID_ID3D10Multithread, (void**)&pD3D10Multithread)))
			break;

		pD3D10Multithread->SetMultithreadProtected(TRUE);

	} while (FALSE);
	SafeRelease(pD3D10Multithread);
	if (FAILED(hr))
		return;

	// IMFDXGIDeviceManager の作成とD3Dデバイスの登録
	if (FAILED(hr = ::MFCreateDXGIDeviceManager(&m_DXGIDeviceManagerID, &m_pDXGIDeviceManager)))
		return;
	m_pDXGIDeviceManager->ResetDevice(m_pD3DDevice, m_DXGIDeviceManagerID);

	// IDXGIOutput の取得
	IDXGIDevice* pDXGIDevice;
	IDXGIAdapter* pDXGIAdapter;
	do
	{
		if (FAILED(hr = m_pD3DDevice->QueryInterface(IID_IDXGIDevice, (void**)&pDXGIDevice)))
			break;

		if (FAILED(hr = pDXGIDevice->GetAdapter(&pDXGIAdapter)))
			break;

		if (FAILED(hr = pDXGIAdapter->EnumOutputs(0, &m_pDXGIOutput)))
			break;
	
	} while (FALSE);
	SafeRelease(pDXGIAdapter);
	SafeRelease(pDXGIDevice);
	if (FAILED(hr))
		return;

	// トポロジーワークキュー設定用ID。IUnknown になるなら、内容はなんでもいい。
	IMFMediaType* pID = NULL;
	::MFCreateMediaType(&pID);
	this->ID_RegistarTopologyWorkQueueWithMMCSS = (IUnknown*)pID;

	// 初期化完了
	m_bInitialized = TRUE;
}
DemoPlay::~DemoPlay()
{
}
void DemoPlay::Dispose()
{
	this->m_bInitialized = FALSE;

	// 再生停止
	if (NULL != m_pMediaSession)
		m_pMediaSession->Stop();

	// 表示中なら終了を待つ。
	::WaitForThreadpoolWorkCallbacks(m_pWork, TRUE);
	::CloseThreadpoolWork(m_pWork);
	
	// MediaFoundation の解放
	SafeRelease(ID_RegistarTopologyWorkQueueWithMMCSS);
	SafeRelease(m_pMediaSinkAttributes);
	SafeRelease(m_pMediaSink);
	SafeRelease(m_pMediaSource);
	SafeRelease(m_pMediaSession);

	// DXGI, D3D11 の解放
	SafeRelease(m_pDXGIDeviceManager);
	SafeRelease(m_pDXGIOutput);
	SafeRelease(m_pDXGISwapChain);
	SafeRelease(m_pD3DDeviceContext);
	SafeRelease(m_pD3DDevice);

	// MediaFoundation のシャットダウン
	::MFShutdown();
}
HRESULT DemoPlay::Play(LPCTSTR szFile)
{
	HRESULT hr = S_OK;

	::ResetEvent(m_evReadyOrFailed);

	// MediaSession を作成する。
	if (FAILED(hr = this->CreateMediaSession(&m_pMediaSession)))
		return hr;

	// ファイルから MediaSource を作成する。
	if (FAILED(hr = this->CreateMediaSourceFromFile(szFile, &m_pMediaSource)))
		return hr;

	IMFTopology* pTopology = NULL;
	do
	{
		// 部分トポロジーを作成。
		if (FAILED(hr = this->CreateTopology(&pTopology)))
			break;

		// MediaSession に登録。
		if (FAILED(hr = m_pMediaSession->SetTopology(0, pTopology)))
			break;

	} while (FALSE);

	SafeRelease(pTopology);

	if (FAILED(hr))
		return hr;

	// MediaSession が完全トポロジーの作成に成功（または失敗）したら MESessionTopologyStatus イベントが発出されるので、それまで待つ。
	::WaitForSingleObject(m_evReadyOrFailed, 5000);

	if (FAILED(m_hrStatus))
		return m_hrStatus;	// 作成失敗

	// MediaSession の再生を開始。
	PROPVARIANT prop;
	::PropVariantInit(&prop);
	m_pMediaSession->Start(NULL, &prop);
	
	return S_OK;
}
void DemoPlay::UpdateAndPresent()
{
	if (!m_bPresentNow )	// m_bPresentNow は atomic_bool
	{
		// 描画処理
		this->Draw();

		// SwapChain 表示
		m_bPresentNow = true;
		::SubmitThreadpoolWork(m_pWork);
	}
	else
	{
		// 進行処理
		this->Update();
	}
}
void DemoPlay::Update()
{
	// todo: 描画以外に行いたい処理
}
void DemoPlay::Draw()
{
	HRESULT hr = S_OK;

	IMFSample* pSample = NULL;
	IMFMediaBuffer* pMediaBuffer = NULL;
	IMFDXGIBuffer* pDXGIBuffer = NULL;
	ID3D11Texture2D* pTexture2D = NULL;
	ID3D11Texture2D* pBackBufferTexture2D = NULL;

	do
	{
		// 現在表示すべき IMFSample が TextureMediaSink の TMS_SAMPLE 属性に設定されているので、それを取得する。
		if (FAILED(hr = m_pMediaSinkAttributes->GetUnknown(TMS_SAMPLE, IID_IMFSample, (void**)&pSample)))
			break;
		if (NULL == pSample)
			break;	// 未設定

		// IMFSample からメディアバッファを取得。
		if (FAILED(hr = pSample->GetBufferByIndex(0, &pMediaBuffer)))
			break;

		// メディアバッファからDXGIバッファを取得。
		if (FAILED(hr = pMediaBuffer->QueryInterface(IID_IMFDXGIBuffer, (void**)&pDXGIBuffer)))
			break;

		// DXGIバッファから転送元 Texture2D を取得。
		if (FAILED(hr = pDXGIBuffer->GetResource(IID_ID3D11Texture2D, (void**)&pTexture2D)))
			break;

		// 転送元 Texture2D のサブリソースインデックスを取得。
		UINT subIndex;
		if (FAILED(hr = pDXGIBuffer->GetSubresourceIndex(&subIndex)))
			break;

		//
		// これで、IMFSample から ID3D11Texture2D を取得することができた。
		// 今回のデモでは、これを単純に SwapChain に矩形描画することにする。
		//

		// SwapChain から転送先 Texture2D を取得。
		if (FAILED(hr = m_pDXGISwapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&pBackBufferTexture2D)))
			break;

		// 転送。
		m_pD3DDeviceContext->CopySubresourceRegion(pBackBufferTexture2D, 0, 0, 0, 0, pTexture2D, subIndex, NULL);

	} while (FALSE);

	// GetUnknown で TMS_SAMPLE を得たとき、その IMFSample は TextureMediaSink から更新されないよう lock されている。
	// この lock を解除するために、TMS_SAMPLE 属性に何か（なんでもいい）を SetUnknwon する。これで IMFSample が TextureMediaSink から更新可能になる。
	m_pMediaSinkAttributes->SetUnknown(TMS_SAMPLE, NULL);

	SafeRelease(pBackBufferTexture2D);
	SafeRelease(pTexture2D);
	SafeRelease(pDXGIBuffer);
	SafeRelease(pMediaBuffer);
	SafeRelease(pSample);
}

HRESULT DemoPlay::CreateMediaSession(IMFMediaSession** ppMediaSession)
{
	HRESULT hr = S_OK;

	IMFMediaSession* pMediaSession = nullptr;
	do
	{
		// MediaSession を生成する。
		if (FAILED(hr = ::MFCreateMediaSession(NULL, &pMediaSession)))
			break;

		// MediaSession からのイベント送信を開始する。
		if (FAILED(hr = pMediaSession->BeginGetEvent(this, nullptr)))
			break;

		// 作成完了。
		(*ppMediaSession) = pMediaSession;
		(*ppMediaSession)->AddRef();
		hr = S_OK;

	} while (FALSE);

	SafeRelease(pMediaSession);

	return hr;
}
HRESULT DemoPlay::CreateMediaSourceFromFile(LPCTSTR szFile, IMFMediaSource** ppMediaSource)
{
	HRESULT hr = S_OK;

	IMFSourceResolver* pResolver = nullptr;
	IMFMediaSource* pMediaSource = nullptr;
	do
	{
		// ソースリゾルバを作成する。
		if (FAILED(hr = ::MFCreateSourceResolver(&pResolver)))
			break;

		// ファイルパスからメディアソースを作成する。
		MF_OBJECT_TYPE type;
		if (FAILED(hr = pResolver->CreateObjectFromURL(szFile, MF_RESOLUTION_MEDIASOURCE, NULL, &type, (IUnknown**)&pMediaSource)))
			break;

		// 作成完了。
		(*ppMediaSource) = pMediaSource;
		(*ppMediaSource)->AddRef();
		hr = S_OK;

	} while (FALSE);

	SafeRelease(pMediaSource);
	SafeRelease(pResolver);

	return hr;
}
HRESULT DemoPlay::CreateTopology(IMFTopology** ppTopology)
{
	HRESULT hr = S_OK;

	IMFTopology* pTopology = NULL;
	IMFPresentationDescriptor* pPresentationDesc = NULL;
	do
	{
		// 新しいトポロジーを作成する。
		if (FAILED(hr = ::MFCreateTopology(&pTopology)))
			break;

		// メディアソース用のプレゼン識別子を作成する。
		if(FAILED(hr = m_pMediaSource->CreatePresentationDescriptor(&pPresentationDesc)))
			break;

		// プレゼン識別子から、メディアソースのストリームの数を取得する。
		DWORD dwDescCount;
		if (FAILED(hr = pPresentationDesc->GetStreamDescriptorCount(&dwDescCount)))
			break;

		// メディアソースの各ストリームについて、トポロジーノードを作成してトポロジーに追加する。
		for (DWORD i = 0; i < dwDescCount; i++)
		{
			if (FAILED(hr = this->AddTopologyNodes(pTopology, pPresentationDesc, i)))
				break;
		}
		if (FAILED(hr))
			break;

		// 作成完了。
		(*ppTopology) = pTopology;
		(*ppTopology)->AddRef();
		hr = S_OK;

	} while (FALSE);

	SafeRelease(pPresentationDesc);
	SafeRelease(pTopology);

	return hr;
}
HRESULT DemoPlay::AddTopologyNodes(IMFTopology* pTopology, IMFPresentationDescriptor* pPresentationDesc, DWORD index)
{
	HRESULT hr = S_OK;

	BOOL bSelected;
	IMFStreamDescriptor* pStreamDesc = NULL;
	IMFTopologyNode* pSourceNode = NULL;
	IMFTopologyNode* pOutputNode = NULL;
	do
	{
		// 指定されたストリーム番号のストリーム記述子を取得する。
		if (FAILED(hr = pPresentationDesc->GetStreamDescriptorByIndex(index, &bSelected, &pStreamDesc)))
			break;

		if (bSelected)
		{
			// (A) ストリームが選択されているなら、トポロジーに追加する。
			if (FAILED(hr = this->CreateSourceNode(pPresentationDesc, pStreamDesc, &pSourceNode)))
				break;

			GUID majorType;
			if (FAILED(hr = this->CreateOutputNode(pStreamDesc, &pOutputNode, &majorType)))
				break;

			if (majorType == MFMediaType_Audio)
			{
				pSourceNode->SetString(MF_TOPONODE_WORKQUEUE_MMCSS_CLASS, _T("Audio"));
				pSourceNode->SetUINT32(MF_TOPONODE_WORKQUEUE_ID, 1);
			}
			if (majorType == MFMediaType_Video)
			{
				pSourceNode->SetString(MF_TOPONODE_WORKQUEUE_MMCSS_CLASS, _T("Playback"));
				pSourceNode->SetUINT32(MF_TOPONODE_WORKQUEUE_ID, 2);
			}

			if (NULL != pSourceNode && NULL != pOutputNode)
			{
				pTopology->AddNode(pSourceNode);
				pTopology->AddNode(pOutputNode);

				pSourceNode->ConnectOutput(0, pOutputNode, 0);
			}
		}
		else
		{
			// (B) ストリームが選択されていないなら、何もしない。
		}

	} while (FALSE);

	SafeRelease(pOutputNode);
	SafeRelease(pSourceNode);
	SafeRelease(pStreamDesc);

	return hr;
}
HRESULT DemoPlay::CreateSourceNode(IMFPresentationDescriptor* pPresentationDesc, IMFStreamDescriptor* pStreamDesc, IMFTopologyNode** ppSourceNode)
{
	HRESULT hr = S_OK;

	IMFTopologyNode* pSourceNode = NULL;
	do
	{
		// ソースノードを作成する。
		if (FAILED(hr = ::MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode)))
			break;

		// ソースノードに３属性を設定する。
		if (FAILED(hr = pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, m_pMediaSource)))
			break;
		if (FAILED(hr = pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPresentationDesc)))
			break;
		if (FAILED(hr = pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pStreamDesc)))
			break;
	
		// 作成完了。
		(*ppSourceNode) = pSourceNode;
		(*ppSourceNode)->AddRef();
		hr = S_OK;

	} while (FALSE);

	SafeRelease(pSourceNode);

	return hr;
}
HRESULT DemoPlay::CreateOutputNode(IMFStreamDescriptor* pStreamDesc, IMFTopologyNode** ppOutputNode, GUID* pMajorType)
{
	HRESULT hr = S_OK;

	IMFTopologyNode* pOutputNode = NULL;
	IMFMediaTypeHandler* pMediaTypeHandler = NULL;
	do
	{
		// 出力ノードを作成。
		if (FAILED(hr = ::MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode)))
			break;

		// メディアハンドラを取得。
		if (FAILED(hr = pStreamDesc->GetMediaTypeHandler(&pMediaTypeHandler)))
			break;

		// メジャータイプを取得。
		GUID majorType;
		if (FAILED(hr = pMediaTypeHandler->GetMajorType(&majorType)))
			break;

		if (majorType == MFMediaType_Video)
		{
			// (A) ビデオレンダラ

			if (NULL == m_pMediaSink)
			{
				// TextureMediaSink を作成。
				if (FAILED(hr = ::CreateD3D11TextureMediaSink(IID_IMFMediaSink, (void**)&m_pMediaSink, m_pDXGIDeviceManager, m_pD3DDevice)))
					break;

				// IMFSample 受け取り用に IMFAttributes を取得しておく。
				if (FAILED(hr = m_pMediaSink->QueryInterface(IID_IMFAttributes, (void**)&m_pMediaSinkAttributes)))
					break;
			}

			IMFStreamSink* pStreamSink = NULL;
			do
			{
				// ストリームシンク #0 を取得。
				if (FAILED(hr = m_pMediaSink->GetStreamSinkById(0, &pStreamSink)))
					break;

				// ストリームシンクを出力ノードに登録。
				if (FAILED(hr = pOutputNode->SetObject(pStreamSink)))
					break;

			} while (FALSE);

			SafeRelease(pStreamDesc);
		}
		else if (majorType == MFMediaType_Audio)
		{
			// (B) オーディオレンダラ

			IMFActivate* pActivate = NULL;
			do
			{
				// 既定のオーディオレンダラのアクティベートを生成。
				if (FAILED(hr = ::MFCreateAudioRendererActivate(&pActivate)))
					break;

				// 出力ノードに登録。
				if (FAILED(hr = pOutputNode->SetObject(pActivate)))
					break;

			} while (FALSE);

			SafeRelease(pActivate);
		}

		// 作成完了。
		(*ppOutputNode) = pOutputNode;
		(*ppOutputNode)->AddRef();
		hr = S_OK;

	} while (FALSE);

	SafeRelease(pMediaTypeHandler);
	SafeRelease(pOutputNode);

	return hr;
}

// IUnknown 実装
ULONG	DemoPlay::AddRef()
{
	return InterlockedIncrement(&this->m_refCount);
}
HRESULT DemoPlay::QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
{
	if (NULL == ppv)
		return E_POINTER;

	if (iid == IID_IUnknown)
	{
		*ppv = static_cast<IUnknown*>(static_cast<IMFAsyncCallback*>(this));
	}
	else if (iid == __uuidof(IMFAsyncCallback))
	{
		*ppv = static_cast<IMFAsyncCallback*>(this);
	}
	else
	{
		*ppv = NULL;
		return E_NOINTERFACE;
	}

	this->AddRef();

	return S_OK;
}
ULONG	DemoPlay::Release()
{
	ULONG uCount = InterlockedDecrement(&this->m_refCount);

	if (uCount == 0)
		delete this;

	return uCount;
}

// IMFAsyncCallback 実装
STDMETHODIMP DemoPlay::GetParameters(__RPC__out DWORD *pdwFlags, __RPC__out DWORD *pdwQueue)
{
	// このメソッドの実装はオプション。
	return E_NOTIMPL;
}
STDMETHODIMP DemoPlay::Invoke(__RPC__in_opt IMFAsyncResult *pAsyncResult)
{
	if (!m_bInitialized)
		return MF_E_SHUTDOWN;

	HRESULT hr = S_OK;

	IUnknown* pUnk;
	if (SUCCEEDED(hr = pAsyncResult->GetState(&pUnk)))
	{
		// (A) State がある（E_POINTERエラーじゃない）場合

		if (this->ID_RegistarTopologyWorkQueueWithMMCSS == pUnk)
		{
			this->OnEndRegistarTopologyWorkQueueWithMMCSS(pAsyncResult);
			return S_OK;
		}
		else
		{
			return E_INVALIDARG;
		}
	}
	else
	{
		// (B) State がない場合

		IMFMediaEvent* pMediaEvent = NULL;
		MediaEventType eventType;
		do
		{
			// MediaSession からのイベントを受信する。
			if (FAILED(hr = m_pMediaSession->EndGetEvent(pAsyncResult, &pMediaEvent)))
				break;
			if (FAILED(hr = pMediaEvent->GetType(&eventType)))
				break;
			if (FAILED(hr = pMediaEvent->GetStatus(&m_hrStatus)))
				break;

			// 結果が失敗なら終了。
			if (FAILED(m_hrStatus))
			{
				::SetEvent(m_evReadyOrFailed);
				return m_hrStatus;
			}

			// イベントタイプ別に分岐。
			switch (eventType)
			{
			case MESessionTopologyStatus:

				// Status 取得。
				UINT32 topoStatus;
				if (FAILED(hr = pMediaEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &topoStatus)))
					break;
				switch (topoStatus)
				{
				case MF_TOPOSTATUS_READY:
					this->OnTopologyReady(pMediaEvent);
					break;
				}
				break;

			case MESessionStarted:
				this->OnSessionStarted(pMediaEvent);
				break;

			case MESessionPaused:
				this->OnSessionPaused(pMediaEvent);
				break;

			case MESessionStopped:
				this->OnSessionStopped(pMediaEvent);
				break;

			case MESessionClosed:
				this->OnSessionClosed(pMediaEvent);
				break;

			case MEEndOfPresentation:
				this->OnPresentationEnded(pMediaEvent);
				break;
			}

		} while (FALSE);

		SafeRelease(pMediaEvent);

		// 次の MediaSession イベントの受信を待つ。
		if (eventType != MESessionClosed)
			hr = m_pMediaSession->BeginGetEvent(this, NULL);

		return hr;
	}
}

void DemoPlay::OnTopologyReady(IMFMediaEvent* pMediaEvent)
{
	HRESULT hr;

	// トポロジーワークキューの MMCSS へのクラス割り当て（非同期処理）を開始する。

	IMFGetService* pGetService = NULL;
	IMFWorkQueueServices* pWorkQueueServices = NULL;
	do
	{
		if (FAILED(hr = this->m_pMediaSession->QueryInterface(IID_IMFGetService, (void**)&pGetService)))
			break;

		if (FAILED(hr = pGetService->GetService(MF_WORKQUEUE_SERVICES, IID_IMFWorkQueueServices, (void**)&pWorkQueueServices)))
			break;

		if (FAILED(hr = pWorkQueueServices->BeginRegisterTopologyWorkQueuesWithMMCSS(this, this->ID_RegistarTopologyWorkQueueWithMMCSS)))
			break;

	} while (FALSE);

	SafeRelease(pWorkQueueServices);
	SafeRelease(pGetService);

	if (FAILED(hr))
	{
		this->m_hrStatus = hr;
		::SetEvent(this->m_evReadyOrFailed);
	}
}
void DemoPlay::OnSessionStarted(IMFMediaEvent* pMediaEvent)
{
}
void DemoPlay::OnSessionPaused(IMFMediaEvent* pMediaEvent)
{
}
void DemoPlay::OnSessionStopped(IMFMediaEvent* pMediaEvent)
{
}
void DemoPlay::OnSessionClosed(IMFMediaEvent* pMediaEvent)
{
}
void DemoPlay::OnPresentationEnded(IMFMediaEvent* pMediaEvent)
{
}
void DemoPlay::OnEndRegistarTopologyWorkQueueWithMMCSS(IMFAsyncResult* pAsyncResult)
{
	HRESULT hr;

	// トポロジーワークキューの MMCSS へのクラス割り当て（非同期処理）を完了する。

	IMFGetService* pGetService = NULL;
	IMFWorkQueueServices* pWorkQueueServices = NULL;
	do
	{
		if (FAILED(hr = this->m_pMediaSession->QueryInterface(IID_IMFGetService, (void**)&pGetService)))
			break;

		if (FAILED(hr = pGetService->GetService(MF_WORKQUEUE_SERVICES, IID_IMFWorkQueueServices, (void**)&pWorkQueueServices)))
			break;

		if (FAILED(hr = pWorkQueueServices->EndRegisterTopologyWorkQueuesWithMMCSS(pAsyncResult)))
			break;

	} while (FALSE);

	SafeRelease(pWorkQueueServices);
	SafeRelease(pGetService);


	if (FAILED(hr))
		this->m_hrStatus = hr;
	else
		this->m_hrStatus = S_OK;

	::SetEvent(this->m_evReadyOrFailed);	// イベント発火。
}

// SwapChain を表示するタスク
void CALLBACK DemoPlay::PresentSwapChainWork(PTP_CALLBACK_INSTANCE pInstance, LPVOID pvParam, PTP_WORK pWork)
{
	auto pDemoPlay = (DemoPlay*)pvParam;

	pDemoPlay->m_pDXGIOutput->WaitForVBlank();
	pDemoPlay->m_pDXGISwapChain->Present(1, 0);

	pDemoPlay->m_bPresentNow = false;
}

