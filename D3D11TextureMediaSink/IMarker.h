#pragma once

namespace D3D11TextureMediaSink
{
	// IMarker:
	// IMFStreamSink::PlaceMarker 呼び出しを非同期にハンドリングするためのカスタムインターフェース。
	//
	// マーカーは、
	// 　・マーカー型		(MFSTREAMSINK_MARKER_TYPE)
	// 　・マーカーデータ	(PROVARIANT)
	// 　・コンテキスト		(PROVARIANT)
	// で構成される。
	//
	// このインターフェースにより、マーカーデータをIUnknownオブジェクトの内側に格納することができ、
	// そのオブジェクトを、そのメディアタイプを保持しているキューと同じキューに維持することができる。
	// これは、サンプルとマーカーはシリアライズされなければならないという理由により、便利である。
	// つまり、その前に来たすべてのサンプルを処理し終わるまで、マーカーについて責任を持つことができない。
	//
	// IMarker は標準 Media Foundation インターフェースではないので注意すること。
	//
	MIDL_INTERFACE("3AC82233-933C-43a9-AF3D-ADC94EABF406")
	IMarker : public IUnknown
	{
		virtual STDMETHODIMP GetType(MFSTREAMSINK_MARKER_TYPE* pType) = 0;
		virtual STDMETHODIMP GetValue(PROPVARIANT* pvar) = 0;
		virtual STDMETHODIMP GetContext(PROPVARIANT* pvar) = 0;
	};
}
