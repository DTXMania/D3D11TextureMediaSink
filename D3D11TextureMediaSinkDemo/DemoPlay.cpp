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

	// MediaFoundation �̃Z�b�g�A�b�v
	::MFStartup(MF_VERSION);

	// ID3D11Device �� IDXGISwapChain �̍쐬
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

	// �}���`�X���b�h���[�h�� ON �ɐݒ肷��BDXVA���g���ꍇ�͕K�{�B
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

	// IMFDXGIDeviceManager �̍쐬��D3D�f�o�C�X�̓o�^
	if (FAILED(hr = ::MFCreateDXGIDeviceManager(&m_DXGIDeviceManagerID, &m_pDXGIDeviceManager)))
		return;
	m_pDXGIDeviceManager->ResetDevice(m_pD3DDevice, m_DXGIDeviceManagerID);

	// IDXGIOutput �̎擾
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

	// �g�|���W�[���[�N�L���[�ݒ�pID�BIUnknown �ɂȂ�Ȃ�A���e�͂Ȃ�ł������B
	IMFMediaType* pID = NULL;
	::MFCreateMediaType(&pID);
	this->ID_RegistarTopologyWorkQueueWithMMCSS = (IUnknown*)pID;

	// ����������
	m_bInitialized = TRUE;
}
DemoPlay::~DemoPlay()
{
}
void DemoPlay::Dispose()
{
	this->m_bInitialized = FALSE;

	// �Đ���~
	if (NULL != m_pMediaSession)
		m_pMediaSession->Stop();

	// �\�����Ȃ�I����҂B
	::WaitForThreadpoolWorkCallbacks(m_pWork, TRUE);
	::CloseThreadpoolWork(m_pWork);
	
	// MediaFoundation �̉��
	SafeRelease(ID_RegistarTopologyWorkQueueWithMMCSS);
	SafeRelease(m_pMediaSinkAttributes);
	SafeRelease(m_pMediaSink);
	SafeRelease(m_pMediaSource);
	SafeRelease(m_pMediaSession);

	// DXGI, D3D11 �̉��
	SafeRelease(m_pDXGIDeviceManager);
	SafeRelease(m_pDXGIOutput);
	SafeRelease(m_pDXGISwapChain);
	SafeRelease(m_pD3DDeviceContext);
	SafeRelease(m_pD3DDevice);

	// MediaFoundation �̃V���b�g�_�E��
	::MFShutdown();
}
HRESULT DemoPlay::Play(LPCTSTR szFile)
{
	HRESULT hr = S_OK;

	::ResetEvent(m_evReadyOrFailed);

	// MediaSession ���쐬����B
	if (FAILED(hr = this->CreateMediaSession(&m_pMediaSession)))
		return hr;

	// �t�@�C������ MediaSource ���쐬����B
	if (FAILED(hr = this->CreateMediaSourceFromFile(szFile, &m_pMediaSource)))
		return hr;

	IMFTopology* pTopology = NULL;
	do
	{
		// �����g�|���W�[���쐬�B
		if (FAILED(hr = this->CreateTopology(&pTopology)))
			break;

		// MediaSession �ɓo�^�B
		if (FAILED(hr = m_pMediaSession->SetTopology(0, pTopology)))
			break;

	} while (FALSE);

	SafeRelease(pTopology);

	if (FAILED(hr))
		return hr;

	// MediaSession �����S�g�|���W�[�̍쐬�ɐ����i�܂��͎��s�j������ MESessionTopologyStatus �C�x���g�����o�����̂ŁA����܂ő҂B
	::WaitForSingleObject(m_evReadyOrFailed, 5000);

	if (FAILED(m_hrStatus))
		return m_hrStatus;	// �쐬���s

	// MediaSession �̍Đ����J�n�B
	PROPVARIANT prop;
	::PropVariantInit(&prop);
	m_pMediaSession->Start(NULL, &prop);
	
	return S_OK;
}
void DemoPlay::UpdateAndPresent()
{
	if (!m_bPresentNow )	// m_bPresentNow �� atomic_bool
	{
		// �`�揈��
		this->Draw();

		// SwapChain �\��
		m_bPresentNow = true;
		::SubmitThreadpoolWork(m_pWork);
	}
	else
	{
		// �i�s����
		this->Update();
	}
}
void DemoPlay::Update()
{
	// todo: �`��ȊO�ɍs����������
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
		// ���ݕ\�����ׂ� IMFSample �� TextureMediaSink �� TMS_SAMPLE �����ɐݒ肳��Ă���̂ŁA������擾����B
		if (FAILED(hr = m_pMediaSinkAttributes->GetUnknown(TMS_SAMPLE, IID_IMFSample, (void**)&pSample)))
			break;
		if (NULL == pSample)
			break;	// ���ݒ�

		// IMFSample ���烁�f�B�A�o�b�t�@���擾�B
		if (FAILED(hr = pSample->GetBufferByIndex(0, &pMediaBuffer)))
			break;

		// ���f�B�A�o�b�t�@����DXGI�o�b�t�@���擾�B
		if (FAILED(hr = pMediaBuffer->QueryInterface(IID_IMFDXGIBuffer, (void**)&pDXGIBuffer)))
			break;

		// DXGI�o�b�t�@����]���� Texture2D ���擾�B
		if (FAILED(hr = pDXGIBuffer->GetResource(IID_ID3D11Texture2D, (void**)&pTexture2D)))
			break;

		// �]���� Texture2D �̃T�u���\�[�X�C���f�b�N�X���擾�B
		UINT subIndex;
		if (FAILED(hr = pDXGIBuffer->GetSubresourceIndex(&subIndex)))
			break;

		//
		// ����ŁAIMFSample ���� ID3D11Texture2D ���擾���邱�Ƃ��ł����B
		// ����̃f���ł́A�����P���� SwapChain �ɋ�`�`�悷�邱�Ƃɂ���B
		//

		// SwapChain ����]���� Texture2D ���擾�B
		if (FAILED(hr = m_pDXGISwapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&pBackBufferTexture2D)))
			break;

		// �]���B
		m_pD3DDeviceContext->CopySubresourceRegion(pBackBufferTexture2D, 0, 0, 0, 0, pTexture2D, subIndex, NULL);

	} while (FALSE);

	// GetUnknown �� TMS_SAMPLE �𓾂��Ƃ��A���� IMFSample �� TextureMediaSink ����X�V����Ȃ��悤 lock ����Ă���B
	// ���� lock ���������邽�߂ɁATMS_SAMPLE �����ɉ����i�Ȃ�ł������j�� SetUnknwon ����B����� IMFSample �� TextureMediaSink ����X�V�\�ɂȂ�B
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
		// MediaSession �𐶐�����B
		if (FAILED(hr = ::MFCreateMediaSession(NULL, &pMediaSession)))
			break;

		// MediaSession ����̃C�x���g���M���J�n����B
		if (FAILED(hr = pMediaSession->BeginGetEvent(this, nullptr)))
			break;

		// �쐬�����B
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
		// �\�[�X���]���o���쐬����B
		if (FAILED(hr = ::MFCreateSourceResolver(&pResolver)))
			break;

		// �t�@�C���p�X���烁�f�B�A�\�[�X���쐬����B
		MF_OBJECT_TYPE type;
		if (FAILED(hr = pResolver->CreateObjectFromURL(szFile, MF_RESOLUTION_MEDIASOURCE, NULL, &type, (IUnknown**)&pMediaSource)))
			break;

		// �쐬�����B
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
		// �V�����g�|���W�[���쐬����B
		if (FAILED(hr = ::MFCreateTopology(&pTopology)))
			break;

		// ���f�B�A�\�[�X�p�̃v���[�����ʎq���쐬����B
		if(FAILED(hr = m_pMediaSource->CreatePresentationDescriptor(&pPresentationDesc)))
			break;

		// �v���[�����ʎq����A���f�B�A�\�[�X�̃X�g���[���̐����擾����B
		DWORD dwDescCount;
		if (FAILED(hr = pPresentationDesc->GetStreamDescriptorCount(&dwDescCount)))
			break;

		// ���f�B�A�\�[�X�̊e�X�g���[���ɂ��āA�g�|���W�[�m�[�h���쐬���ăg�|���W�[�ɒǉ�����B
		for (DWORD i = 0; i < dwDescCount; i++)
		{
			if (FAILED(hr = this->AddTopologyNodes(pTopology, pPresentationDesc, i)))
				break;
		}
		if (FAILED(hr))
			break;

		// �쐬�����B
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
		// �w�肳�ꂽ�X�g���[���ԍ��̃X�g���[���L�q�q���擾����B
		if (FAILED(hr = pPresentationDesc->GetStreamDescriptorByIndex(index, &bSelected, &pStreamDesc)))
			break;

		if (bSelected)
		{
			// (A) �X�g���[�����I������Ă���Ȃ�A�g�|���W�[�ɒǉ�����B
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
			// (B) �X�g���[�����I������Ă��Ȃ��Ȃ�A�������Ȃ��B
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
		// �\�[�X�m�[�h���쐬����B
		if (FAILED(hr = ::MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode)))
			break;

		// �\�[�X�m�[�h�ɂR������ݒ肷��B
		if (FAILED(hr = pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, m_pMediaSource)))
			break;
		if (FAILED(hr = pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPresentationDesc)))
			break;
		if (FAILED(hr = pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pStreamDesc)))
			break;
	
		// �쐬�����B
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
		// �o�̓m�[�h���쐬�B
		if (FAILED(hr = ::MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode)))
			break;

		// ���f�B�A�n���h�����擾�B
		if (FAILED(hr = pStreamDesc->GetMediaTypeHandler(&pMediaTypeHandler)))
			break;

		// ���W���[�^�C�v���擾�B
		GUID majorType;
		if (FAILED(hr = pMediaTypeHandler->GetMajorType(&majorType)))
			break;

		if (majorType == MFMediaType_Video)
		{
			// (A) �r�f�I�����_��

			if (NULL == m_pMediaSink)
			{
				// TextureMediaSink ���쐬�B
				if (FAILED(hr = ::CreateD3D11TextureMediaSink(IID_IMFMediaSink, (void**)&m_pMediaSink, m_pDXGIDeviceManager, m_pD3DDevice)))
					break;

				// IMFSample �󂯎��p�� IMFAttributes ���擾���Ă����B
				if (FAILED(hr = m_pMediaSink->QueryInterface(IID_IMFAttributes, (void**)&m_pMediaSinkAttributes)))
					break;
			}

			IMFStreamSink* pStreamSink = NULL;
			do
			{
				// �X�g���[���V���N #0 ���擾�B
				if (FAILED(hr = m_pMediaSink->GetStreamSinkById(0, &pStreamSink)))
					break;

				// �X�g���[���V���N���o�̓m�[�h�ɓo�^�B
				if (FAILED(hr = pOutputNode->SetObject(pStreamSink)))
					break;

			} while (FALSE);

			SafeRelease(pStreamDesc);
		}
		else if (majorType == MFMediaType_Audio)
		{
			// (B) �I�[�f�B�I�����_��

			IMFActivate* pActivate = NULL;
			do
			{
				// ����̃I�[�f�B�I�����_���̃A�N�e�B�x�[�g�𐶐��B
				if (FAILED(hr = ::MFCreateAudioRendererActivate(&pActivate)))
					break;

				// �o�̓m�[�h�ɓo�^�B
				if (FAILED(hr = pOutputNode->SetObject(pActivate)))
					break;

			} while (FALSE);

			SafeRelease(pActivate);
		}

		// �쐬�����B
		(*ppOutputNode) = pOutputNode;
		(*ppOutputNode)->AddRef();
		hr = S_OK;

	} while (FALSE);

	SafeRelease(pMediaTypeHandler);
	SafeRelease(pOutputNode);

	return hr;
}

// IUnknown ����
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

// IMFAsyncCallback ����
STDMETHODIMP DemoPlay::GetParameters(__RPC__out DWORD *pdwFlags, __RPC__out DWORD *pdwQueue)
{
	// ���̃��\�b�h�̎����̓I�v�V�����B
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
		// (A) State ������iE_POINTER�G���[����Ȃ��j�ꍇ

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
		// (B) State ���Ȃ��ꍇ

		IMFMediaEvent* pMediaEvent = NULL;
		MediaEventType eventType;
		do
		{
			// MediaSession ����̃C�x���g����M����B
			if (FAILED(hr = m_pMediaSession->EndGetEvent(pAsyncResult, &pMediaEvent)))
				break;
			if (FAILED(hr = pMediaEvent->GetType(&eventType)))
				break;
			if (FAILED(hr = pMediaEvent->GetStatus(&m_hrStatus)))
				break;

			// ���ʂ����s�Ȃ�I���B
			if (FAILED(m_hrStatus))
			{
				::SetEvent(m_evReadyOrFailed);
				return m_hrStatus;
			}

			// �C�x���g�^�C�v�ʂɕ���B
			switch (eventType)
			{
			case MESessionTopologyStatus:

				// Status �擾�B
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

		// ���� MediaSession �C�x���g�̎�M��҂B
		if (eventType != MESessionClosed)
			hr = m_pMediaSession->BeginGetEvent(this, NULL);

		return hr;
	}
}

void DemoPlay::OnTopologyReady(IMFMediaEvent* pMediaEvent)
{
	HRESULT hr;

	// �g�|���W�[���[�N�L���[�� MMCSS �ւ̃N���X���蓖�āi�񓯊������j���J�n����B

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

	// �g�|���W�[���[�N�L���[�� MMCSS �ւ̃N���X���蓖�āi�񓯊������j����������B

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

	::SetEvent(this->m_evReadyOrFailed);	// �C�x���g���΁B
}

// SwapChain ��\������^�X�N
void CALLBACK DemoPlay::PresentSwapChainWork(PTP_CALLBACK_INSTANCE pInstance, LPVOID pvParam, PTP_WORK pWork)
{
	auto pDemoPlay = (DemoPlay*)pvParam;

	pDemoPlay->m_pDXGIOutput->WaitForVBlank();
	pDemoPlay->m_pDXGISwapChain->Present(1, 0);

	pDemoPlay->m_bPresentNow = false;
}

