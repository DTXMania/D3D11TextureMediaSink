#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	Presenter::Presenter()
	{
		this->_Shutdown済み = FALSE;
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

		// サンプルアロケーターの初期化を試みる。
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

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		UINT32 フレームレートの分子 = 30000, フレームレートの分母 = 1001;
		UINT32 画像の幅px, 画像の高さpx = 0;

		// 検査対象のメディアタイプからフレームサイズ（画像の幅、高さ）を取得する。
		if (FAILED(hr = ::MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &画像の幅px, &画像の高さpx)))
			return hr;

		// 検査対象のメディアタイプからフレームレート（分子、分母）を取得する。
		::MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &フレームレートの分子, &フレームレートの分母);

		// フォーマットがサポート可能か確認する。
		D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
		ZeroMemory(&ContentDesc, sizeof(ContentDesc));
		ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
		ContentDesc.InputWidth = (DWORD)画像の幅px;
		ContentDesc.InputHeight = (DWORD)画像の高さpx;
		ContentDesc.OutputWidth = (DWORD)画像の幅px;
		ContentDesc.OutputHeight = (DWORD)画像の高さpx;
		ContentDesc.InputFrameRate.Numerator = フレームレートの分子;
		ContentDesc.InputFrameRate.Denominator = フレームレートの分母;
		ContentDesc.OutputFrameRate.Numerator = フレームレートの分子;
		ContentDesc.OutputFrameRate.Denominator = フレームレートの分母;
		ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

		// ID3D11VideoProcessorEnumerator を取得する。
		ID3D11VideoProcessorEnumerator*	pVideoProcessorEnum = NULL;
		if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessorEnumerator(&ContentDesc, &pVideoProcessorEnum)))
			return hr;

		// 引数に指定されたフォーマットが入力としてサポート可能かをチェックする。
		UINT uiFlags;
		hr = pVideoProcessorEnum->CheckVideoProcessorFormat(dxgiFormat, &uiFlags);
		if (FAILED(hr) || 0 == (uiFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT))
			return MF_E_UNSUPPORTED_D3D_TYPE;

		return hr;
	}
	HRESULT Presenter::SetCurrentMediaType(IMFMediaType* pMediaType)
	{
		HRESULT hr = S_OK;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;


		//
		// todo: 必要あれば、ここで解像度やアスペクト比などを取得する。
		//



		// メディアタイプからフレームサイズ（画像の幅、高さ）を取得する。
		if (FAILED(hr = ::MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &this->_Width, &this->_Height)))
			return hr;

		// メディアタイプからフォーマットGUIDを取得する。
		GUID subType;
		if (FAILED(hr = pMediaType->GetGUID(MF_MT_SUBTYPE, &subType)))
			return hr;

		// サンプルアロケーターの初期化を試みる。
		this->InitializeSampleAllocator();

		return S_OK;
	}
	BOOL Presenter::IsReadyNextSample()
	{
		return this->_次のサンプルを処理してよい;
	}

	void Presenter::Shutdown()
	{
		AutoLock(this->_csPresenter);

		if (!this->_Shutdown済み)
		{
			//this._XVPControl = null;
			//this._XVP = null;

			SafeRelease(this->_DXGIDeviceManager);
			SafeRelease(this->_D3D11VideoDevice);
			SafeRelease(this->_D3D11Device);

			this->_Shutdown済み = TRUE;
		}
	}
	HRESULT Presenter::Flush()
	{
		AutoLock lock(this->_csPresenter);

		HRESULT hr;

		// シャットダウン済み？
		if (FAILED(hr = this->CheckShutdown()))
			return hr;

		this->_次のサンプルを処理してよい = TRUE;

		return S_OK;
	}
	HRESULT Presenter::ProcessFrame(_In_ IMFMediaType* pCurrentType, _In_ IMFSample* pSample, _Out_ UINT32* punInterlaceMode, _Out_ BOOL* pbDeviceChanged, _Out_ BOOL* pbProcessAgain, _Out_ IMFSample** ppOutputSample)
	{
		HRESULT hr;
		*pbDeviceChanged = FALSE;
		*pbProcessAgain = FALSE;

		// シャットダウン済み？
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
			// 入力サンプル (IMFSample) から IMFMediaBuffer を取得。
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

			// 入力サンプルのインタレースモードをメディアタイプから取得する。
			MFVideoInterlaceMode unInterlaceMode = (MFVideoInterlaceMode) ::MFGetAttributeUINT32(pCurrentType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

			// メディアタイプのインタレースモードが Mix モードの場合、今回の入力サンプルはどれであるかを決定する。
			if (MFVideoInterlace_MixedInterlaceOrProgressive == unInterlaceMode)
			{
				if (FALSE == ::MFGetAttributeUINT32(pSample, MFSampleExtension_Interlaced, FALSE))
				{
					*punInterlaceMode = MFVideoInterlace_Progressive;	// プログレッシブである
				}
				else
				{
					if (::MFGetAttributeUINT32(pSample, MFSampleExtension_BottomFieldFirst, FALSE))
					{
						*punInterlaceMode = MFVideoInterlace_FieldInterleavedLowerFirst;	// Lower優先のインターリーブである
					}
					else
					{
						*punInterlaceMode = MFVideoInterlace_FieldInterleavedUpperFirst;	// Upper優先のインターリーブである
					}
				}
			}

			// IMFMediaBuffer から IMFDXGIBuffer を取得。
			if (FAILED(hr = pBuffer->QueryInterface(__uuidof(IMFDXGIBuffer), (LPVOID*)&pMFDXGIBuffer)))
				break;

			// IMFDXGIBuffer から ID3D11Texture2D と次元を取得。
			if (FAILED(hr = pMFDXGIBuffer->GetResource(__uuidof(ID3D11Texture2D), (LPVOID*)&pTexture2D)))
				break;
			UINT dwViewIndex = 0;
			if (FAILED(hr = pMFDXGIBuffer->GetSubresourceIndex(&dwViewIndex)))
				break;

			// ID3D11Texture2D をビデオ処理して、出力サンプル（IMFSample）を得る。
			if (FAILED(hr = this->ProcessFrameUsingD3D11(pTexture2D, dwViewIndex, *punInterlaceMode, ppOutputSample)))
				break;

			// 出力サンプルに、入力サンプルの時刻情報をコピーする。
			LONGLONG llv;
			if (SUCCEEDED(pSample->GetSampleTime(&llv)))
			{
				(*ppOutputSample)->SetSampleTime(llv);
				//TCHAR buf[1024];
				//wsprintf(buf, L"Presenter::ProcessFrame; 表示時刻 %d ms (%x)\n", (llv / 10000), *ppOutputSample);
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
		return (this->_Shutdown済み) ? MF_E_SHUTDOWN : S_OK;
	}
	HRESULT Presenter::InitializeSampleAllocator()
	{
		// デバイスとサイズとフォーマット、そろってる？
		if (NULL == this->_D3D11Device || this->_Width == 0 || this->_Height == 0)
			return MF_E_NOT_INITIALIZED;	// まだそろってない

		this->_SampleAllocator->Shutdown();	// 念のためシャットダウンして、
		return this->_SampleAllocator->Initialize(this->_D3D11Device, this->_Width, this->_Height);	// 初期化。
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
			// ID3D11DeviceContext を取得する。
			this->_D3D11Device->GetImmediateContext(&pDeviceContext);

			// ID3D11VideoContext を取得する。
			if (FAILED(hr = pDeviceContext->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&pVideoContext)))
				break;

			// 入力テクスチャの情報を取得する。
			D3D11_TEXTURE2D_DESC surfaceDesc;
			pTexture2D->GetDesc(&surfaceDesc);

			if (surfaceDesc.Width != this->_Width ||
				surfaceDesc.Height != this->_Height)
			{
				hr = MF_E_INVALID_STREAM_DATA;	// メディアタイプで指定されたサイズと違うなら無視。
				break;
			}

			// ビデオプロセッサの作成がまだなら作成する。
			if (NULL == this->_D3D11VideoProcessorEnum || NULL == this->_D3D11VideoProcessor)
			{
				SafeRelease(this->_D3D11VideoProcessor);		// 念のため
				SafeRelease(this->_D3D11VideoProcessorEnum);	//

				// VideoProcessorEnumerator を作成する。
				D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
				ZeroMemory(&ContentDesc, sizeof(ContentDesc));
				ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
				ContentDesc.InputWidth = surfaceDesc.Width;				// 入力サイズと
				ContentDesc.InputHeight = surfaceDesc.Height;			//
				ContentDesc.OutputWidth = surfaceDesc.Width;			// 出力サイズは同じ
				ContentDesc.OutputHeight = surfaceDesc.Height;			//
				ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;	// 用途：再生
				if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessorEnumerator(&ContentDesc, &this->_D3D11VideoProcessorEnum)))
					break;

				// ビデオプロセッサが DXGI_FORMAT_B8G8R8A8_UNORM を出力でサポートできないならアウト。
				UINT uiFlags;
				DXGI_FORMAT VP_Output_Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				if (FAILED(hr = this->_D3D11VideoProcessorEnum->CheckVideoProcessorFormat(VP_Output_Format, &uiFlags)))
					return hr;
				if (0 == (uiFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
				{
					hr = MF_E_UNSUPPORTED_D3D_TYPE;	// 出力としてサポートできない。
					break;
				}

				// BOBビデオプロセッサのインデックスを検索する。

				DWORD indexOfBOB;
				if (FAILED(hr = this->BOBビデオプロセッサを検索する(&indexOfBOB)))
					break;	// BOBがない...

				// BOBビデオプロセッサを作成する。
				if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessor(this->_D3D11VideoProcessorEnum, indexOfBOB, &this->_D3D11VideoProcessor)))
					break;
			}

			// 出力サンプルを取得する。
			pOutputSample = NULL;
			if (FAILED(hr = this->_SampleAllocator->GetSample(&pOutputSample)))
				break;
			if (FAILED(hr = pOutputSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_UPDATING)))
				break;

			// 出力テクスチャを取得する。
			IMFMediaBuffer* pBuffer = NULL;
			IMFDXGIBuffer* pMFDXGIBuffer = NULL;
			do
			{
				// 出力サンプルからメディアバッファを取得。
				if (FAILED(hr = pOutputSample->GetBufferByIndex(0, &pBuffer)))
					break;

				// IMFMediaBuffer から IMFDXGIBuffer を取得。
				if (FAILED(hr = pBuffer->QueryInterface(__uuidof(IMFDXGIBuffer), (LPVOID*)&pMFDXGIBuffer)))
					break;

				// IMFDXGIBuffer から ID3D11Texture2D を取得。
				if (FAILED(hr = pMFDXGIBuffer->GetResource(__uuidof(ID3D11Texture2D), (LPVOID*)&pOutputTexture2D)))
					break;

			} while (FALSE);

			SafeRelease(pMFDXGIBuffer);
			SafeRelease(pBuffer);

			if (FAILED(hr))
				break;

			// 入力テクスチャから入力ビューを作成する。
			D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputLeftViewDesc;
			ZeroMemory(&InputLeftViewDesc, sizeof(InputLeftViewDesc));
			InputLeftViewDesc.FourCC = 0;
			InputLeftViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
			InputLeftViewDesc.Texture2D.MipSlice = 0;
			InputLeftViewDesc.Texture2D.ArraySlice = dwViewIndex;
			if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessorInputView(pTexture2D, this->_D3D11VideoProcessorEnum, &InputLeftViewDesc, &pInputView)))
				break;

			// 出力テクスチャから出力ビューを作成する。
			D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
			ZeroMemory(&OutputViewDesc, sizeof(OutputViewDesc));
			OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
			OutputViewDesc.Texture2D.MipSlice = 0;
			OutputViewDesc.Texture2DArray.MipSlice = 0;
			OutputViewDesc.Texture2DArray.FirstArraySlice = 0;
			if (FAILED(hr = this->_D3D11VideoDevice->CreateVideoProcessorOutputView(pOutputTexture2D, this->_D3D11VideoProcessorEnum, &OutputViewDesc, &pOutputView)))
				break;

			// ビデオコンテキストのパラメータ設定を行う。
			{
				// 入力ストリーム 0 のフォーマットを設定。
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

				// 入力ストリーム 0 の出力レートを設定。
				pVideoContext->VideoProcessorSetStreamOutputRate(this->_D3D11VideoProcessor, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, TRUE, NULL);

				// 入力ストリーム 0 の転送元矩形を設定。入力サーフェス内の矩形。入力サーフェスを基準としたピクセル座標。（入力ストリームの数だけ存在）
				RECT 転送元矩形 = { 0L, 0L, (LONG)surfaceDesc.Width, (LONG)surfaceDesc.Height };
				pVideoContext->VideoProcessorSetStreamSourceRect(this->_D3D11VideoProcessor, 0, TRUE, &転送元矩形);

				// 入力ストリーム 0 の転送先矩形を設定。出力サーフェス内の矩形。出力サーフェスを基準としたピクセル座標。（入力ストリームの数だけ存在）
				RECT 転送先矩形 = { 0L, 0L, (LONG)surfaceDesc.Width, (LONG)surfaceDesc.Height };
				pVideoContext->VideoProcessorSetStreamDestRect(this->_D3D11VideoProcessor, 0, TRUE, &転送先矩形);

				// 出力ターゲット矩形を設定。出力サーフェス内の矩形。出力サーフェスを基準としたピクセル座標。（１つだけ存在）
				RECT ターゲット矩形 = { 0L, 0L, (LONG)surfaceDesc.Width, (LONG)surfaceDesc.Height };
				pVideoContext->VideoProcessorSetOutputTargetRect(this->_D3D11VideoProcessor, TRUE, &ターゲット矩形);

				// 入力ストリーム 0 の入力色空間を設定。
				D3D11_VIDEO_PROCESSOR_COLOR_SPACE colorSpace = {};
				colorSpace.YCbCr_xvYCC = 1;
				pVideoContext->VideoProcessorSetStreamColorSpace(this->_D3D11VideoProcessor, 0, &colorSpace);

				// 出力色空間を設定。
				pVideoContext->VideoProcessorSetOutputColorSpace(this->_D3D11VideoProcessor, &colorSpace);

				// 出力背景色を黒に設定。
				D3D11_VIDEO_COLOR backgroundColor = {};
				backgroundColor.RGBA.A = 1.0F;
				backgroundColor.RGBA.R = 1.0F * static_cast<float>(GetRValue(0)) / 255.0F;
				backgroundColor.RGBA.G = 1.0F * static_cast<float>(GetGValue(0)) / 255.0F;
				backgroundColor.RGBA.B = 1.0F * static_cast<float>(GetBValue(0)) / 255.0F;
				pVideoContext->VideoProcessorSetOutputBackgroundColor(this->_D3D11VideoProcessor, FALSE, &backgroundColor);
			}

			// 入力から出力へ、加工しながら変換する。
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

			// 出力サンプルが正常に作成できたので、返す。
			if (NULL != ppVideoOutFrame)
			{
				*ppVideoOutFrame = pOutputSample;
				//(*ppVideoOutFrame)->AddRef();	--> SampleAllocatorの保有するサンプルはAddRef/Releaseしない
			}

		} while (FALSE);

		SafeRelease(pInputView);
		SafeRelease(pOutputView);
		SafeRelease(pOutputTexture2D);
		//SafeRelease(pOutputSample);	--> 同上
		SafeRelease(pVideoContext);
		SafeRelease(pDeviceContext);

		return hr;
	}
	HRESULT Presenter::BOBビデオプロセッサを検索する(_Out_ DWORD* pIndex)
	{
		// ※ BOBはいかなる参照フレームも要求しないので、プログレッシブ／インターレースビデオの両方で使うことができる。

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

			// インターレース解除がサポートされていればその番号を返す。
			if (0 != (convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB))
			{
				*pIndex = i;
				return S_OK;	// 戻る
			}
		}

		return E_FAIL;	// なかった
	}
}
