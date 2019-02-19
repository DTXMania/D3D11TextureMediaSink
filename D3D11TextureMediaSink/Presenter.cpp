#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	Presenter::Presenter()
	{
		this->_Shutdown�ς� = FALSE;
		this->_csPresenter = new CriticalSection();
		this->_SampleAllocator = new SampleAllocator();
	}
	Presenter::~Presenter()
	{
		this->_csPresenter = NULL;
	}

	void Presenter::SetD3D11(void* pDXGIDeviceManager, void* pD3D11Device)
	{
		this->_DXGIDeviceManager = (IMFDXGIDeviceManager*)pDXGIDeviceManager;
		this->_DXGIDeviceManager->AddRef();

		this->_D3D11Device = (ID3D11Device*)pD3D11Device;
		this->_D3D11Device->AddRef();

		this->_D3D11VideoDevice = NULL;
		this->_D3D11Device->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&this->_D3D11VideoDevice);

		// �T���v���A���P�[�^�[�̏����������݂�B
		this->InitializeSampleAllocator();
	}
	IMFDXGIDeviceManager* Presenter::GetDXGIDeviceManager()
	{
		this->_DXGIDeviceManager->AddRef();
		return this->_DXGIDeviceManager;
	}
	ID3D11Device* Presenter::GetD3D11Device()
	{
		this->_D3D11Device->AddRef();
		return this->_D3D11Device;
	}
	HRESULT Presenter::IsSupported(IMFMediaType* pMediaType, DXGI_FORMAT dxgiFormat)
	{
		HRESULT hr = S_OK;

		if (NULL == pMediaType)
			return E_POINTER;

		// �V���b�g�_�E���ς݁H
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		UINT32 �t���[�����[�g�̕��q = 30000, �t���[�����[�g�̕��� = 1001;
		UINT32 �摜�̕�px, �摜�̍���px = 0;

		// �����Ώۂ̃��f�B�A�^�C�v����t���[���T�C�Y�i�摜�̕��A�����j���擾����B
		if (FAILED(hr = ::MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &�摜�̕�px, &�摜�̍���px)))
			return hr;

		// �����Ώۂ̃��f�B�A�^�C�v����t���[�����[�g�i���q�A����j���擾����B
		::MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &�t���[�����[�g�̕��q, &�t���[�����[�g�̕���);

		// �t�H�[�}�b�g���T�|�[�g�\���m�F����B
		D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
		ZeroMemory(&ContentDesc, sizeof(ContentDesc));
		ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
		ContentDesc.InputWidth = (DWORD)�摜�̕�px;
		ContentDesc.InputHeight = (DWORD)�摜�̍���px;
		ContentDesc.OutputWidth = (DWORD)�摜�̕�px;
		ContentDesc.OutputHeight = (DWORD)�摜�̍���px;
		ContentDesc.InputFrameRate.Numerator = �t���[�����[�g�̕��q;
		ContentDesc.InputFrameRate.Denominator = �t���[�����[�g�̕���;
		ContentDesc.OutputFrameRate.Numerator = �t���[�����[�g�̕��q;
		ContentDesc.OutputFrameRate.Denominator = �t���[�����[�g�̕���;
		ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

		// ID3D11VideoProcessorEnumerator ���擾����B
		ID3D11VideoProcessorEnumerator*	pVideoProcessorEnum = NULL;
		if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessorEnumerator(&ContentDesc, &pVideoProcessorEnum)))
			return hr;

		// �����Ɏw�肳�ꂽ�t�H�[�}�b�g�����͂Ƃ��ăT�|�[�g�\�����`�F�b�N����B
		UINT uiFlags;
		hr = pVideoProcessorEnum->CheckVideoProcessorFormat(dxgiFormat, &uiFlags);
		if (FAILED(hr) || 0 == (uiFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT))
			return MF_E_UNSUPPORTED_D3D_TYPE;

		return hr;
	}
	HRESULT Presenter::SetCurrentMediaType(IMFMediaType* pMediaType)
	{
		HRESULT hr = S_OK;

		// �V���b�g�_�E���ς݁H
		if (FAILED(hr = this->CheckShutdown()))
			return hr;


		//
		// todo: �K�v����΁A�����ŉ𑜓x��A�X�y�N�g��Ȃǂ��擾����B
		//



		// ���f�B�A�^�C�v����t���[���T�C�Y�i�摜�̕��A�����j���擾����B
		if (FAILED(hr = ::MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &this->_Width, &this->_Height)))
			return hr;

		// ���f�B�A�^�C�v����t�H�[�}�b�gGUID���擾����B
		GUID subType;
		if (FAILED(hr = pMediaType->GetGUID(MF_MT_SUBTYPE, &subType)))
			return hr;

		// �T���v���A���P�[�^�[�̏����������݂�B
		this->InitializeSampleAllocator();

		return S_OK;
	}
	BOOL Presenter::IsReadyNextSample()
	{
		return this->_���̃T���v�����������Ă悢;
	}

	void Presenter::Shutdown()
	{
		AutoLock(this->_csPresenter);

		if (!this->_Shutdown�ς�)
		{
			//this._XVPControl = null;
			//this._XVP = null;

			SafeRelease(this->_DXGIDeviceManager);
			SafeRelease(this->_D3D11VideoDevice);
			SafeRelease(this->_D3D11Device);

			this->_Shutdown�ς� = TRUE;
		}
	}
	HRESULT Presenter::Flush()
	{
		AutoLock lock(this->_csPresenter);

		HRESULT hr;

		// �V���b�g�_�E���ς݁H
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		this->_���̃T���v�����������Ă悢 = TRUE;

		return S_OK;
	}
	HRESULT Presenter::ProcessFrame(_In_ IMFMediaType* pCurrentType, _In_ IMFSample* pSample, _Out_ UINT32* punInterlaceMode, _Out_ BOOL* pbDeviceChanged, _Out_ BOOL* pbProcessAgain, _Out_ IMFSample** ppOutputSample)
	{
		HRESULT hr;
		*pbDeviceChanged = FALSE;
		*pbProcessAgain = FALSE;

		// �V���b�g�_�E���ς݁H
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (NULL == pCurrentType || NULL == pSample || NULL == punInterlaceMode || NULL == pbDeviceChanged || NULL == pbProcessAgain || NULL == ppOutputSample)
			return E_POINTER;


		IMFMediaBuffer* pBuffer = NULL;
		IMFDXGIBuffer* pMFDXGIBuffer = NULL;
		ID3D11Texture2D* pTexture2D = NULL;
		IMFSample* pRTSample = NULL;

		do
		{
			// ���̓T���v�� (IMFSample) ���� IMFMediaBuffer ���擾�B
			DWORD cBuffers = 0;
			if (FAILED(hr = pSample->GetBufferCount(&cBuffers)))
				break;
			if (1 == cBuffers)
			{
				if (FAILED(hr = pSample->GetBufferByIndex(0, &pBuffer)))
					break;
			}
			else
			{
				if (FAILED(hr = pSample->ConvertToContiguousBuffer(&pBuffer)))
					break;
			}

			// ���̓T���v���̃C���^���[�X���[�h�����f�B�A�^�C�v����擾����B
			MFVideoInterlaceMode unInterlaceMode = (MFVideoInterlaceMode) ::MFGetAttributeUINT32(pCurrentType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

			// ���f�B�A�^�C�v�̃C���^���[�X���[�h�� Mix ���[�h�̏ꍇ�A����̓��̓T���v���͂ǂ�ł��邩�����肷��B
			if (MFVideoInterlace_MixedInterlaceOrProgressive == unInterlaceMode)
			{
				if (FALSE == ::MFGetAttributeUINT32(pSample, MFSampleExtension_Interlaced, FALSE))
				{
					*punInterlaceMode = MFVideoInterlace_Progressive;	// �v���O���b�V�u�ł���
				}
				else
				{
					if (::MFGetAttributeUINT32(pSample, MFSampleExtension_BottomFieldFirst, FALSE))
					{
						*punInterlaceMode = MFVideoInterlace_FieldInterleavedLowerFirst;	// Lower�D��̃C���^�[���[�u�ł���
					}
					else
					{
						*punInterlaceMode = MFVideoInterlace_FieldInterleavedUpperFirst;	// Upper�D��̃C���^�[���[�u�ł���
					}
				}
			}

			// IMFMediaBuffer ���� IMFDXGIBuffer ���擾�B
			if (FAILED(hr = pBuffer->QueryInterface(__uuidof(IMFDXGIBuffer), (LPVOID*)&pMFDXGIBuffer)))
				break;

			// IMFDXGIBuffer ���� ID3D11Texture2D �Ǝ������擾�B
			if (FAILED(hr = pMFDXGIBuffer->GetResource(__uuidof(ID3D11Texture2D), (LPVOID*)&pTexture2D)))
				break;
			UINT dwViewIndex = 0;
			if (FAILED(hr = pMFDXGIBuffer->GetSubresourceIndex(&dwViewIndex)))
				break;

			// ID3D11Texture2D ���r�f�I�������āA�o�̓T���v���iIMFSample�j�𓾂�B
			if (FAILED(hr = this->ProcessFrameUsingD3D11(pTexture2D, dwViewIndex, *punInterlaceMode, ppOutputSample)))
				break;

			// �o�̓T���v���ɁA���̓T���v���̎��������R�s�[����B
			LONGLONG llv;
			if (SUCCEEDED(pSample->GetSampleTime(&llv)))
			{
				(*ppOutputSample)->SetSampleTime(llv);
				//TCHAR buf[1024];
				//wsprintf(buf, L"Presenter::ProcessFrame; �\������ %d ms (%x)\n", (llv / 10000), *ppOutputSample);
				//OutputDebugString(buf);
			}
			if (SUCCEEDED(pSample->GetSampleDuration(&llv)))
			{
				(*ppOutputSample)->SetSampleDuration(llv);
			}
			DWORD flags;
			if (SUCCEEDED(pSample->GetSampleFlags(&flags)))
				(*ppOutputSample)->SetSampleFlags(flags);

		} while (FALSE);

		SafeRelease(pTexture2D);
		SafeRelease(pMFDXGIBuffer);
		SafeRelease(pBuffer);

		return S_OK;
	}
	HRESULT Presenter::ReleaseSample(IMFSample* pSample)
	{
		return this->_SampleAllocator->ReleaseSample(pSample);
	}

	// private

	HRESULT Presenter::CheckShutdown() const
	{
		return (this->_Shutdown�ς�) ? MF_E_SHUTDOWN : S_OK;
	}
	HRESULT Presenter::InitializeSampleAllocator()
	{
		// �f�o�C�X�ƃT�C�Y�ƃt�H�[�}�b�g�A������Ă�H
		if (NULL == this->_D3D11Device || this->_Width == 0 || this->_Height == 0)
			return MF_E_NOT_INITIALIZED;	// �܂�������ĂȂ�

		this->_SampleAllocator->Shutdown();	// �O�̂��߃V���b�g�_�E�����āA
		return this->_SampleAllocator->Initialize(this->_D3D11Device, this->_Width, this->_Height);	// �������B
	}
	HRESULT Presenter::ProcessFrameUsingD3D11(ID3D11Texture2D* pTexture2D, UINT dwViewIndex, UINT32 unInterlaceMode, IMFSample** ppVideoOutFrame)
	{
		HRESULT hr = S_OK;
		ID3D11DeviceContext* pDeviceContext = NULL;
		ID3D11VideoContext* pVideoContext = NULL;
		IMFSample* pOutputSample = NULL;
		ID3D11Texture2D* pOutputTexture2D = NULL;
		ID3D11VideoProcessorOutputView* pOutputView = NULL;
		ID3D11VideoProcessorInputView* pInputView = NULL;

		do
		{
			// ID3D11DeviceContext ���擾����B
			this->_D3D11Device->GetImmediateContext(&pDeviceContext);

			// ID3D11VideoContext ���擾����B
			if (FAILED(hr = pDeviceContext->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&pVideoContext)))
				break;

			// ���̓e�N�X�`���̏����擾����B
			D3D11_TEXTURE2D_DESC surfaceDesc;
			pTexture2D->GetDesc(&surfaceDesc);

			if (surfaceDesc.Width != this->_Width ||
				surfaceDesc.Height != this->_Height)
			{
				hr = MF_E_INVALID_STREAM_DATA;	// ���f�B�A�^�C�v�Ŏw�肳�ꂽ�T�C�Y�ƈႤ�Ȃ疳���B
				break;
			}

			// �r�f�I�v���Z�b�T�̍쐬���܂��Ȃ�쐬����B
			if (NULL == this->_D3D11VideoProcessorEnum || NULL == this->_D3D11VideoProcessor)
			{
				SafeRelease(this->_D3D11VideoProcessor);		// �O�̂���
				SafeRelease(this->_D3D11VideoProcessorEnum);	//

				// VideoProcessorEnumerator ���쐬����B
				D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
				ZeroMemory(&ContentDesc, sizeof(ContentDesc));
				ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
				ContentDesc.InputWidth = surfaceDesc.Width;				// ���̓T�C�Y��
				ContentDesc.InputHeight = surfaceDesc.Height;			//
				ContentDesc.OutputWidth = surfaceDesc.Width;			// �o�̓T�C�Y�͓���
				ContentDesc.OutputHeight = surfaceDesc.Height;			//
				ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;	// �p�r�F�Đ�
				if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessorEnumerator(&ContentDesc, &this->_D3D11VideoProcessorEnum)))
					break;

				// �r�f�I�v���Z�b�T�� DXGI_FORMAT_B8G8R8A8_UNORM ���o�͂ŃT�|�[�g�ł��Ȃ��Ȃ�A�E�g�B
				UINT uiFlags;
				DXGI_FORMAT VP_Output_Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				if (FAILED(hr = this->_D3D11VideoProcessorEnum->CheckVideoProcessorFormat(VP_Output_Format, &uiFlags)))
					return hr;
				if (0 == (uiFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
				{
					hr = MF_E_UNSUPPORTED_D3D_TYPE;	// �o�͂Ƃ��ăT�|�[�g�ł��Ȃ��B
					break;
				}

				// BOB�r�f�I�v���Z�b�T�̃C���f�b�N�X����������B

				DWORD indexOfBOB;
				if (FAILED(hr = this->BOB�r�f�I�v���Z�b�T����������(&indexOfBOB)))
					break;	// BOB���Ȃ�...

				// BOB�r�f�I�v���Z�b�T���쐬����B
				if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessor(this->_D3D11VideoProcessorEnum, indexOfBOB, &this->_D3D11VideoProcessor)))
					break;
			}

			// �o�̓T���v�����擾����B
			pOutputSample = NULL;
			if (FAILED(hr = this->_SampleAllocator->GetSample(&pOutputSample)))
				break;
			if (FAILED(hr = pOutputSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_UPDATING)))
				break;

			// �o�̓e�N�X�`�����擾����B
			IMFMediaBuffer* pBuffer = NULL;
			IMFDXGIBuffer* pMFDXGIBuffer = NULL;
			do
			{
				// �o�̓T���v�����烁�f�B�A�o�b�t�@���擾�B
				if (FAILED(hr = pOutputSample->GetBufferByIndex(0, &pBuffer)))
					break;

				// IMFMediaBuffer ���� IMFDXGIBuffer ���擾�B
				if (FAILED(hr = pBuffer->QueryInterface(__uuidof(IMFDXGIBuffer), (LPVOID*)&pMFDXGIBuffer)))
					break;

				// IMFDXGIBuffer ���� ID3D11Texture2D ���擾�B
				if (FAILED(hr = pMFDXGIBuffer->GetResource(__uuidof(ID3D11Texture2D), (LPVOID*)&pOutputTexture2D)))
					break;

			} while (FALSE);

			SafeRelease(pMFDXGIBuffer);
			SafeRelease(pBuffer);

			if (FAILED(hr))
				break;

			// ���̓e�N�X�`��������̓r���[���쐬����B
			D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputLeftViewDesc;
			ZeroMemory(&InputLeftViewDesc, sizeof(InputLeftViewDesc));
			InputLeftViewDesc.FourCC = 0;
			InputLeftViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
			InputLeftViewDesc.Texture2D.MipSlice = 0;
			InputLeftViewDesc.Texture2D.ArraySlice = dwViewIndex;
			if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessorInputView(pTexture2D, this->_D3D11VideoProcessorEnum, &InputLeftViewDesc, &pInputView)))
				break;

			// �o�̓e�N�X�`������o�̓r���[���쐬����B
			D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
			ZeroMemory(&OutputViewDesc, sizeof(OutputViewDesc));
			OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
			OutputViewDesc.Texture2D.MipSlice = 0;
			OutputViewDesc.Texture2DArray.MipSlice = 0;
			OutputViewDesc.Texture2DArray.FirstArraySlice = 0;
			if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessorOutputView(pOutputTexture2D, this->_D3D11VideoProcessorEnum, &OutputViewDesc, &pOutputView)))
				break;

			// �r�f�I�R���e�L�X�g�̃p�����[�^�ݒ���s���B
			{
				// ���̓X�g���[�� 0 �̃t�H�[�}�b�g��ݒ�B
				D3D11_VIDEO_FRAME_FORMAT FrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
				if ((MFVideoInterlace_FieldInterleavedUpperFirst == unInterlaceMode) ||
					(MFVideoInterlace_FieldSingleUpper == unInterlaceMode) ||
					(MFVideoInterlace_MixedInterlaceOrProgressive == unInterlaceMode))
				{
					FrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
				}
				else if ((MFVideoInterlace_FieldInterleavedLowerFirst == unInterlaceMode) ||
					(MFVideoInterlace_FieldSingleLower == unInterlaceMode))
				{
					FrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
				}
				pVideoContext->VideoProcessorSetStreamFrameFormat(this->_D3D11VideoProcessor, 0, FrameFormat);

				// ���̓X�g���[�� 0 �̏o�̓��[�g��ݒ�B
				pVideoContext->VideoProcessorSetStreamOutputRate(this->_D3D11VideoProcessor, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, TRUE, NULL);

				// ���̓X�g���[�� 0 �̓]������`��ݒ�B���̓T�[�t�F�X���̋�`�B���̓T�[�t�F�X����Ƃ����s�N�Z�����W�B�i���̓X�g���[���̐��������݁j
				RECT �]������` = { 0L, 0L, (LONG)surfaceDesc.Width, (LONG)surfaceDesc.Height };
				pVideoContext->VideoProcessorSetStreamSourceRect(this->_D3D11VideoProcessor, 0, TRUE, &�]������`);

				// ���̓X�g���[�� 0 �̓]�����`��ݒ�B�o�̓T�[�t�F�X���̋�`�B�o�̓T�[�t�F�X����Ƃ����s�N�Z�����W�B�i���̓X�g���[���̐��������݁j
				RECT �]�����` = { 0L, 0L, (LONG)surfaceDesc.Width, (LONG)surfaceDesc.Height };
				pVideoContext->VideoProcessorSetStreamDestRect(this->_D3D11VideoProcessor, 0, TRUE, &�]�����`);

				// �o�̓^�[�Q�b�g��`��ݒ�B�o�̓T�[�t�F�X���̋�`�B�o�̓T�[�t�F�X����Ƃ����s�N�Z�����W�B�i�P�������݁j
				RECT �^�[�Q�b�g��` = { 0L, 0L, (LONG)surfaceDesc.Width, (LONG)surfaceDesc.Height };
				pVideoContext->VideoProcessorSetOutputTargetRect(this->_D3D11VideoProcessor, TRUE, &�^�[�Q�b�g��`);

				// ���̓X�g���[�� 0 �̓��͐F��Ԃ�ݒ�B
				D3D11_VIDEO_PROCESSOR_COLOR_SPACE colorSpace = {};
				colorSpace.YCbCr_xvYCC = 1;
				pVideoContext->VideoProcessorSetStreamColorSpace(this->_D3D11VideoProcessor, 0, &colorSpace);

				// �o�͐F��Ԃ�ݒ�B
				pVideoContext->VideoProcessorSetOutputColorSpace(this->_D3D11VideoProcessor, &colorSpace);

				// �o�͔w�i�F�����ɐݒ�B
				D3D11_VIDEO_COLOR backgroundColor = {};
				backgroundColor.RGBA.A = 1.0F;
				backgroundColor.RGBA.R = 1.0F * static_cast<float>(GetRValue(0)) / 255.0F;
				backgroundColor.RGBA.G = 1.0F * static_cast<float>(GetGValue(0)) / 255.0F;
				backgroundColor.RGBA.B = 1.0F * static_cast<float>(GetBValue(0)) / 255.0F;
				pVideoContext->VideoProcessorSetOutputBackgroundColor(this->_D3D11VideoProcessor, FALSE, &backgroundColor);
			}

			// ���͂���o�͂ցA���H���Ȃ���ϊ�����B
			D3D11_VIDEO_PROCESSOR_STREAM StreamData;
			ZeroMemory(&StreamData, sizeof(StreamData));
			StreamData.Enable = TRUE;
			StreamData.OutputIndex = 0;
			StreamData.InputFrameOrField = 0;
			StreamData.PastFrames = 0;
			StreamData.FutureFrames = 0;
			StreamData.ppPastSurfaces = NULL;
			StreamData.ppFutureSurfaces = NULL;
			StreamData.pInputSurface = pInputView;
			StreamData.ppPastSurfacesRight = NULL;
			StreamData.ppFutureSurfacesRight = NULL;
			if (FAILED(hr = pVideoContext->VideoProcessorBlt(this->_D3D11VideoProcessor, pOutputView, 0, 1, &StreamData)))
				break;

			// �o�̓T���v��������ɍ쐬�ł����̂ŁA�Ԃ��B
			if (NULL != ppVideoOutFrame)
			{
				*ppVideoOutFrame = pOutputSample;
				//(*ppVideoOutFrame)->AddRef();	--> SampleAllocator�ۗ̕L����T���v����AddRef/Release���Ȃ�
			}

		} while (FALSE);

		SafeRelease(pInputView);
		SafeRelease(pOutputView);
		SafeRelease(pOutputTexture2D);
		//SafeRelease(pOutputSample);	--> ����
		SafeRelease(pVideoContext);
		SafeRelease(pDeviceContext);

		return hr;
	}
	HRESULT Presenter::BOB�r�f�I�v���Z�b�T����������(_Out_ DWORD* pIndex)
	{
		// �� BOB�͂����Ȃ�Q�ƃt���[�����v�����Ȃ��̂ŁA�v���O���b�V�u�^�C���^�[���[�X�r�f�I�̗����Ŏg�����Ƃ��ł���B

		HRESULT hr = S_OK;
		D3D11_VIDEO_PROCESSOR_CAPS caps = {};

		*pIndex = 0;

		if (FAILED(hr = this->_D3D11VideoProcessorEnum->GetVideoProcessorCaps(&caps)))
			return hr;

		for (DWORD i = 0; i < caps.RateConversionCapsCount; i++)
		{
			D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS convCaps = {};
			if (FAILED(hr = this->_D3D11VideoProcessorEnum->GetVideoProcessorRateConversionCaps(i, &convCaps)))
				return hr;

			// �C���^�[���[�X�������T�|�[�g����Ă���΂��̔ԍ���Ԃ��B
			if (0 != (convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB))
			{
				*pIndex = i;
				return S_OK;	// �߂�
			}
		}

		return E_FAIL;	// �Ȃ�����
	}
}
