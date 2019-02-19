#pragma once

namespace D3D11TextureMediaSink
{
	class Presenter
	{
	public:
		Presenter();
		~Presenter();

		BOOL IsReadyNextSample();
		void SetD3D11(void* pDXGIDeviceManager, void* pD3D11Device);
		IMFDXGIDeviceManager* GetDXGIDeviceManager();
		ID3D11Device* GetD3D11Device();
		HRESULT IsSupported(IMFMediaType* pMediaType, DXGI_FORMAT dxgiFormat);
		HRESULT SetCurrentMediaType(IMFMediaType* pMediaType);
		void Shutdown();
		HRESULT Flush();
		HRESULT ProcessFrame(IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample = NULL);
		HRESULT ReleaseSample(IMFSample* pSample);

	private:
		BOOL _Shutdown済み = FALSE;
		BOOL _次のサンプルを処理してよい = TRUE;
		UINT32 _Width = 0;
		UINT32 _Height = 0;
		IMFDXGIDeviceManager*	_DXGIDeviceManager = NULL;
		ID3D11Device*			_D3D11Device = NULL;
		ID3D11VideoDevice*      _D3D11VideoDevice = NULL;
		ID3D11VideoProcessorEnumerator* _D3D11VideoProcessorEnum = NULL;
		ID3D11VideoProcessor* _D3D11VideoProcessor = NULL;
		SampleAllocator* _SampleAllocator = NULL;

		CriticalSection* _csPresenter = NULL;


		HRESULT CheckShutdown() const;
		HRESULT InitializeSampleAllocator();
		HRESULT ProcessFrameUsingD3D11(ID3D11Texture2D* pTexture2D, UINT dwViewIndex, UINT32 unInterlaceMode, IMFSample** ppVideoOutFrame);
		HRESULT BOBビデオプロセッサを検索する(_Out_ DWORD* pIndex);
	};
}
