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

		this->_空きができた = ::CreateEvent(NULL, FALSE, FALSE, NULL);	// 通常はFALSEなので注意

		for (int i = 0; i < SAMPLE_MAX; i++)
		{
			IMFSample* pSample = NULL;
			ID3D11Texture2D* pTexture = NULL;
			IMFMediaBuffer* pBuffer = NULL;

			do
			{
				// サンプルを作成。
				if (FAILED(hr = ::MFCreateSample(&pSample)))
					break;

				// テクスチャリソースを作成。
				D3D11_TEXTURE2D_DESC desc;
				ZeroMemory(&desc, sizeof(desc));
				desc.ArraySize = 1;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				desc.CPUAccessFlags = 0;
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;	// 固定
				desc.Width = width;		// 指定された幅
				desc.Height = height;	// 指定された高さ
				desc.MipLevels = 1;
				desc.SampleDesc = { 1, 0 };
				desc.Usage = D3D11_USAGE_DEFAULT;
				if (FAILED(hr = pD3DDevice->CreateTexture2D(&desc, NULL, &pTexture)))
					break;

				// テクスチャからメディアバッファを作成。
				if (FAILED(hr = ::MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), pTexture, 0, FALSE, &pBuffer)))
					break;

				// メディアバッファをサンプルに追加。
				if (FAILED(hr = pSample->AddBuffer(pBuffer)))
					break;

				// サンプルの状態を READY に初期化。
				if (FAILED(hr = pSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_READY)))
					break;

				// 完成したサンプルをキューに追加。
				this->_SampleQueue[i] = pSample;
				this->_SampleQueue[i]->AddRef();

			} while (FALSE);

			SafeRelease(pBuffer);
			SafeRelease(pTexture);
			SafeRelease(pSample);

			if (FAILED(hr))
				return hr;
		}

		this->_Shutdown済み = FALSE;

		return S_OK;
	}
	HRESULT SampleAllocator::Shutdown()
	{
		AutoLock lock(&this->_csSampleAllocator);

		if (this->_Shutdown済み)
			return S_FALSE;

		this->_Shutdown済み = TRUE;

		for (int i = 0; i < SAMPLE_MAX; i++)
			this->_SampleQueue[i]->Release();

		return S_OK;
	}
	HRESULT SampleAllocator::CheckShutdown()
	{
		return (this->_Shutdown済み) ? MF_E_SHUTDOWN : S_OK;
	}

	HRESULT SampleAllocator::GetSample(IMFSample** ppSample)
	{
		HRESULT hr = S_OK;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (NULL == ppSample)
			return E_POINTER;

		// 空いてるサンプルを探す。
		while (TRUE)
		{
			int status[SAMPLE_MAX];

			{
				AutoLock lock(&this->_csSampleAllocator);

				for (int i = 0; i < SAMPLE_MAX; i++)
				{
					// SAMPLE_STATE 属性の値が READY なら、空いているサンプルである。

					UINT32 state;
					if (FAILED(hr = this->_SampleQueue[i]->GetUINT32(SAMPLE_STATE, &state)))
						return hr;

					status[i] = state;

					if (state == SAMPLE_STATE_READY)
					{
						*ppSample = this->_SampleQueue[i];	// 空いてたので貸し出し。
						(*ppSample)->AddRef();

						//TCHAR buf[1024];
						//wsprintf(buf, L"SampleAllocator::GetSample - [%d](%X)OK!\n", i, *ppSample);
						//OutputDebugString(buf);

						return S_OK;
					}
				}

			} // lockスコープここまで

			// 空いてなかったので空くまで待つ。
			if (::WaitForSingleObject(this->_空きができた, 5000) == WAIT_TIMEOUT)
				break;	// タイムアウトしたら諦める
		}

		//OutputDebugString(L"SampleAllocator::GetSample - NoSample...\n");
		return MF_E_NOT_FOUND;
	}
	HRESULT SampleAllocator::ReleaseSample(IMFSample* pSample)
	{
		AutoLock lock(&this->_csSampleAllocator);

		HRESULT hr = S_OK;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		if (NULL == pSample)
			return E_POINTER;

		// 貸し出したサンプルを探す。
		for (int i = 0; i < SAMPLE_MAX; i++)
		{
			if (this->_SampleQueue[i] == pSample)
			{
				//pSample->Release();	--> 再利用するんでReleaseしない

				// SAMPLE_STATE 属性を READY にリセット。
				if (FAILED(hr = pSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_READY)))
					return hr;

				//TCHAR buf[1024];
				//wsprintf(buf, L"SampleAllocator::ReleaseSample - [%d]OK!\n", i);
				//OutputDebugString(buf);

				// 空きができた通知
				::SetEvent(this->_空きができた);

				return S_OK;
			}
		}

		//OutputDebugString(L"SampleAllocator::ReleaseSample - No Sample...\n");
		return MF_E_NOT_FOUND;	// うちの貸し出したサンプルじゃない
	}
}
