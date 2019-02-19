#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	Marker::Marker(MFSTREAMSINK_MARKER_TYPE 型)
	{
		this->参照カウンタ = 1;
		this->型 = 型;
		::PropVariantInit(&this->値);
		::PropVariantInit(&this->コンテキスト);
	}
	Marker::~Marker()
	{
		::PropVariantClear(&this->値);
		::PropVariantClear(&this->コンテキスト);
	}

	// static

	HRESULT Marker::Create(
		MFSTREAMSINK_MARKER_TYPE	型,
		const PROPVARIANT*			値,			// NULL にできる。
		const PROPVARIANT*			コンテキスト,	// NULL にできる。
		IMarker**					ppMarker	// [out] 作成した IMarker を受け取るポインタ。
	)
	{
		if (NULL == ppMarker)
			return E_POINTER;

		HRESULT hr = S_OK;

		Marker* pMarker = new Marker(型);
		do
		{
			// 指定された型をもって、Marker を新規生成。
			if (NULL == pMarker)
			{
				hr = E_OUTOFMEMORY;
				break;
			}

			// 指定された値をMarkerの値にコピー。
			if (NULL != 値)
				if (FAILED(hr = ::PropVariantCopy(&pMarker->値, 値)))
					break;

			// 指定されたコンテキストをCMarkerのコンテキストにコピー。
			if (コンテキスト)
				if (FAILED(hr = ::PropVariantCopy(&pMarker->コンテキスト, コンテキスト)))
					break;

			// 戻り値用に参照カウンタを更新して、完成。
			*ppMarker = pMarker;
			(*ppMarker)->AddRef();

		} while (FALSE);

		SafeRelease(pMarker);

		return hr;
	}

	// IUnknown 実装
	ULONG Marker::AddRef()
	{
		return InterlockedIncrement(&this->参照カウンタ);
	}
	ULONG Marker::Release()
	{
		// スレッドセーフにするために、テンポラリ変数に移した値を取得し、返す。
		ULONG uCount = InterlockedDecrement(&this->参照カウンタ);

		if (uCount == 0)
			delete this;

		return uCount;
	}
	HRESULT Marker::QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
	{
		if (!ppv)
			return E_POINTER;

		if (iid == IID_IUnknown)
		{
			*ppv = static_cast<IUnknown*>(this);
		}
		else if (iid == __uuidof(IMarker))
		{
			*ppv = static_cast<IMarker*>(this);
		}
		else
		{
			*ppv = NULL;
			return E_NOINTERFACE;
		}

		this->AddRef();

		return S_OK;
	}

	// IMarker 実装
	HRESULT Marker::GetType(MFSTREAMSINK_MARKER_TYPE* pType)
	{
		if (pType == NULL)
			return E_POINTER;

		*pType = this->型;

		return S_OK;
	}
	HRESULT Marker::GetValue(PROPVARIANT* pvar)
	{
		if (pvar == NULL)
			return E_POINTER;

		return ::PropVariantCopy(pvar, &(this->値));

	}
	HRESULT Marker::GetContext(PROPVARIANT* pvar)
	{
		if (pvar == NULL)
			return E_POINTER;

		return ::PropVariantCopy(pvar, &(this->コンテキスト));
	}
}
