#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	SampleAllocator::SampleAllocator()
	{
	}
	SampleAllocator::~SampleAllocator()
	{
	}

	HRESULT SampleAllocator::Initialize(ID3D11Device* pD3DDevice, int width, int height)
	{
		HRESULT hr = S_OK;

		this->_�󂫂��ł��� = ::CreateEvent(NULL, FALSE, FALSE, NULL);	// �ʏ��FALSE�Ȃ̂Œ���

		for (int i = 0; i < SAMPLE_MAX; i++)
		{
			IMFSample* pSample = NULL;
			ID3D11Texture2D* pTexture = NULL;
			IMFMediaBuffer* pBuffer = NULL;

			do
			{
				// �T���v�����쐬�B
				if (FAILED(hr = ::MFCreateSample(&pSample)))
					break;

				// �e�N�X�`�����\�[�X���쐬�B
				D3D11_TEXTURE2D_DESC desc;
				ZeroMemory(&desc, sizeof(desc));
				desc.ArraySize = 1;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				desc.CPUAccessFlags = 0;
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;	// �Œ�
				desc.Width = width;		// �w�肳�ꂽ��
				desc.Height = height;	// �w�肳�ꂽ����
				desc.MipLevels = 1;
				desc.SampleDesc = { 1, 0 };
				desc.Usage = D3D11_USAGE_DEFAULT;
				if (FAILED(hr = pD3DDevice->CreateTexture2D(&desc, NULL, &pTexture)))
					break;

				// �e�N�X�`�����烁�f�B�A�o�b�t�@���쐬�B
				if (FAILED(hr = ::MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), pTexture, 0, FALSE, &pBuffer)))
					break;

				// ���f�B�A�o�b�t�@���T���v���ɒǉ��B
				if (FAILED(hr = pSample->AddBuffer(pBuffer)))
					break;

				// �T���v���̏�Ԃ� READY �ɏ������B
				if (FAILED(hr = pSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_READY)))
					break;

				// ���������T���v�����L���[�ɒǉ��B
				this->_SampleQueue[i] = pSample;
				this->_SampleQueue[i]->AddRef();

			} while (FALSE);

			SafeRelease(pBuffer);
			SafeRelease(pTexture);
			SafeRelease(pSample);

			if (FAILED(hr))
				return hr;
		}

		this->_Shutdown�ς� = FALSE;

		return S_OK;
	}
	HRESULT SampleAllocator::Shutdown()
	{
		AutoLock lock(&this->_csSampleAllocator);

		if (this->_Shutdown�ς�)
			return S_FALSE;

		this->_Shutdown�ς� = TRUE;

		for (int i = 0; i < SAMPLE_MAX; i++)
			this->_SampleQueue[i]->Release();

		return S_OK;
	}
	HRESULT SampleAllocator::CheckShutdown()
	{
		return (this->_Shutdown�ς�) ? MF_E_SHUTDOWN : S_OK;
	}

	HRESULT SampleAllocator::GetSample(IMFSample** ppSample)
	{
		HRESULT hr = S_OK;

		// �V���b�g�_�E���ς݁H
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (NULL == ppSample)
			return E_POINTER;

		// �󂢂Ă�T���v����T���B
		while (TRUE)
		{
			int status[SAMPLE_MAX];

			{
				AutoLock lock(&this->_csSampleAllocator);

				for (int i = 0; i < SAMPLE_MAX; i++)
				{
					// SAMPLE_STATE �����̒l�� READY �Ȃ�A�󂢂Ă���T���v���ł���B

					UINT32 state;
					if (FAILED(hr = this->_SampleQueue[i]->GetUINT32(SAMPLE_STATE, &state)))
						return hr;

					status[i] = state;

					if (state == SAMPLE_STATE_READY)
					{
						*ppSample = this->_SampleQueue[i];	// �󂢂Ă��̂ő݂��o���B
						(*ppSample)->AddRef();

						//TCHAR buf[1024];
						//wsprintf(buf, L"SampleAllocator::GetSample - [%d](%X)OK!\n", i, *ppSample);
						//OutputDebugString(buf);

						return S_OK;
					}
				}

			} // lock�X�R�[�v�����܂�

			// �󂢂ĂȂ������̂ŋ󂭂܂ő҂B
			if (::WaitForSingleObject(this->_�󂫂��ł���, 5000) == WAIT_TIMEOUT)
				break;	// �^�C���A�E�g��������߂�
		}

		//OutputDebugString(L"SampleAllocator::GetSample - NoSample...\n");
		return MF_E_NOT_FOUND;
	}
	HRESULT SampleAllocator::ReleaseSample(IMFSample* pSample)
	{
		AutoLock lock(&this->_csSampleAllocator);

		HRESULT hr = S_OK;

		// �V���b�g�_�E���ς݁H
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (NULL == pSample)
			return E_POINTER;

		// �݂��o�����T���v����T���B
		for (int i = 0; i < SAMPLE_MAX; i++)
		{
			if (this->_SampleQueue[i] == pSample)
			{
				//pSample->Release();	--> �ė��p������Release���Ȃ�

				// SAMPLE_STATE ������ READY �Ƀ��Z�b�g�B
				if (FAILED(hr = pSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_READY)))
					return hr;

				//TCHAR buf[1024];
				//wsprintf(buf, L"SampleAllocator::ReleaseSample - [%d]OK!\n", i);
				//OutputDebugString(buf);

				// �󂫂��ł����ʒm
				::SetEvent(this->_�󂫂��ł���);

				return S_OK;
			}
		}

		//OutputDebugString(L"SampleAllocator::ReleaseSample - No Sample...\n");
		return MF_E_NOT_FOUND;	// �����݂̑��o�����T���v������Ȃ�
	}
}
