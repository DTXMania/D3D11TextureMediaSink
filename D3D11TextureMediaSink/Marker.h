#pragma once

namespace D3D11TextureMediaSink
{
	// IMFStreamSink::PlaceMarker 用にマーカー情報を保持するクラス。
	//
	class Marker : public IMarker
	{
	public:
		static HRESULT Create(
			MFSTREAMSINK_MARKER_TYPE eMarkerType,
			const PROPVARIANT* pvarMarkerValue,
			const PROPVARIANT* pvarContextValue,
			IMarker** ppMarker);

		// IUnknown 宣言
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv);

		// IMarker 宣言
		STDMETHODIMP GetType(MFSTREAMSINK_MARKER_TYPE* pType);
		STDMETHODIMP GetValue(PROPVARIANT* pvar);
		STDMETHODIMP GetContext(PROPVARIANT* pvar);

	protected:
		MFSTREAMSINK_MARKER_TYPE	型;
		PROPVARIANT					値;
		PROPVARIANT					コンテキスト;

	private:
		Marker(MFSTREAMSINK_MARKER_TYPE 型);
		virtual ~Marker();

		long 参照カウンタ;
	};
}
