#pragma once

namespace D3D11TextureMediaSink
{
	// IMFStreamSink::PlaceMarker �p�Ƀ}�[�J�[����ێ�����N���X�B
	//
	class Marker : public IMarker
	{
	public:
		static HRESULT Create(
			MFSTREAMSINK_MARKER_TYPE eMarkerType,
			const PROPVARIANT* pvarMarkerValue,
			const PROPVARIANT* pvarContextValue,
			IMarker** ppMarker);

		// IUnknown �錾
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv);

		// IMarker �錾
		STDMETHODIMP GetType(MFSTREAMSINK_MARKER_TYPE* pType);
		STDMETHODIMP GetValue(PROPVARIANT* pvar);
		STDMETHODIMP GetContext(PROPVARIANT* pvar);

	protected:
		MFSTREAMSINK_MARKER_TYPE	�^;
		PROPVARIANT					�l;
		PROPVARIANT					�R���e�L�X�g;

	private:
		Marker(MFSTREAMSINK_MARKER_TYPE �^);
		virtual ~Marker();

		long �Q�ƃJ�E���^;
	};
}
