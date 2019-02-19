#pragma once

namespace D3D11TextureMediaSink
{
	// IMFAttributes インターフェースの基本実装。
	// IMFAttributes を実装したクラスは、このクラスを実装して、必要なメソッドをオーバーライドする。
	// 
	template <class TBase = IMFAttributes>
	class MFAttributesImpl : public TBase
	{
	protected:
		MFAttributesImpl()
		{
			this->_AttributeStore = NULL;
		}
		~MFAttributesImpl()
		{
			SafeRelease(this->_AttributeStore);
		}

		HRESULT Initialize(UINT32 初期サイズ = 0)
		{
			return (NULL != this->_AttributeStore) ?
				S_OK :														// 作成済み
				::MFCreateAttributes(&this->_AttributeStore, 初期サイズ);	// 新規作成
		}

	public:

		// IMFAttributes 実装

		STDMETHODIMP GetItem(__RPC__in REFGUID guidKey, __RPC__inout_opt PROPVARIANT* pValue)
		{
			return this->_AttributeStore->GetItem(guidKey, pValue);
		}
		STDMETHODIMP GetItemType(__RPC__in REFGUID guidKey, __RPC__out MF_ATTRIBUTE_TYPE* pType)
		{
			return this->_AttributeStore->GetItemType(guidKey, pType);
		}
		STDMETHODIMP CompareItem(__RPC__in REFGUID guidKey, __RPC__in REFPROPVARIANT Value, __RPC__out BOOL* pbResult)
		{
			return this->_AttributeStore->CompareItem(guidKey, Value, pbResult);
		}
		STDMETHODIMP Compare(__RPC__in_opt IMFAttributes* pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, __RPC__out BOOL* pbResult)
		{
			return this->_AttributeStore->Compare(pTheirs, MatchType, pbResult);
		}
		STDMETHODIMP GetUINT32(__RPC__in REFGUID guidKey, __RPC__out UINT32* punValue)
		{
			return this->_AttributeStore->GetUINT32(guidKey, punValue);
		}
		STDMETHODIMP GetUINT64(__RPC__in REFGUID guidKey, __RPC__out UINT64* punValue)
		{
			return this->_AttributeStore->GetUINT64(guidKey, punValue);
		}
		STDMETHODIMP GetDouble(__RPC__in REFGUID guidKey, __RPC__out double* pfValue)
		{
			return this->_AttributeStore->GetDouble(guidKey, pfValue);
		}
		STDMETHODIMP GetGUID(__RPC__in REFGUID guidKey, __RPC__out GUID* pguidValue)
		{
			return this->_AttributeStore->GetGUID(guidKey, pguidValue);
		}
		STDMETHODIMP GetStringLength(__RPC__in REFGUID guidKey, __RPC__out UINT32* pcchLength)
		{
			return this->_AttributeStore->GetStringLength(guidKey, pcchLength);
		}
		STDMETHODIMP GetString(__RPC__in REFGUID guidKey, __RPC__out_ecount_full(cchBufSize) LPWSTR pwszValue, UINT32 cchBufSize, __RPC__inout_opt UINT32* pcchLength)
		{
			return this->_AttributeStore->GetString(guidKey, pwszValue, cchBufSize, pcchLength);
		}
		STDMETHODIMP GetAllocatedString(__RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt((*pcchLength + 1)) LPWSTR* ppwszValue, __RPC__out UINT32* pcchLength)
		{
			return this->_AttributeStore->GetAllocatedString(guidKey, ppwszValue, pcchLength);
		}
		STDMETHODIMP GetBlobSize(__RPC__in REFGUID guidKey, __RPC__out UINT32* pcbBlobSize)
		{
			return this->_AttributeStore->GetBlobSize(guidKey, pcbBlobSize);
		}
		STDMETHODIMP GetBlob(__RPC__in REFGUID guidKey, __RPC__out_ecount_full(cbBufSize) UINT8* pBuf, UINT32 cbBufSize, __RPC__inout_opt UINT32* pcbBlobSize)
		{
			return this->_AttributeStore->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize);
		}
		STDMETHODIMP GetAllocatedBlob(__RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt(*pcbSize) UINT8** ppBuf, __RPC__out UINT32* pcbSize)
		{
			return this->_AttributeStore->GetAllocatedBlob(guidKey, ppBuf, pcbSize);
		}
		STDMETHODIMP GetUnknown(__RPC__in REFGUID guidKey, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppv)
		{
			return this->_AttributeStore->GetUnknown(guidKey, riid, ppv);
		}
		STDMETHODIMP SetItem(__RPC__in REFGUID guidKey, __RPC__in REFPROPVARIANT Value)
		{
			return this->_AttributeStore->SetItem(guidKey, Value);
		}
		STDMETHODIMP DeleteItem(__RPC__in REFGUID guidKey)
		{
			return this->_AttributeStore->DeleteItem(guidKey);
		}
		STDMETHODIMP DeleteAllItems(void)
		{
			return this->_AttributeStore->DeleteAllItems();
		}
		STDMETHODIMP SetUINT32(__RPC__in REFGUID guidKey, UINT32 unValue)
		{
			return this->_AttributeStore->SetUINT32(guidKey, unValue);
		}
		STDMETHODIMP SetUINT64(__RPC__in REFGUID guidKey, UINT64 unValue)
		{
			return this->_AttributeStore->SetUINT64(guidKey, unValue);
		}
		STDMETHODIMP SetDouble(__RPC__in REFGUID guidKey, double fValue)
		{
			return this->_AttributeStore->SetDouble(guidKey, fValue);
		}
		STDMETHODIMP SetGUID(__RPC__in REFGUID guidKey, __RPC__in REFGUID guidValue)
		{
			return this->_AttributeStore->SetGUID(guidKey, guidValue);
		}
		STDMETHODIMP SetString(__RPC__in REFGUID guidKey, __RPC__in_string LPCWSTR wszValue)
		{
			return this->_AttributeStore->SetString(guidKey, wszValue);
		}
		STDMETHODIMP SetBlob(__RPC__in REFGUID guidKey, __RPC__in_ecount_full(cbBufSize) const UINT8* pBuf, UINT32 cbBufSize)
		{
			return this->_AttributeStore->SetBlob(guidKey, pBuf, cbBufSize);
		}
		STDMETHODIMP SetUnknown(__RPC__in REFGUID guidKey, __RPC__in_opt IUnknown* pUnknown)
		{
			return this->_AttributeStore->SetUnknown(guidKey, pUnknown);
		}
		STDMETHODIMP LockStore(void)
		{
			return this->_AttributeStore->LockStore();
		}
		STDMETHODIMP UnlockStore(void)
		{
			return this->_AttributeStore->UnlockStore();
		}
		STDMETHODIMP GetCount(__RPC__out UINT32* pcItems)
		{
			return this->_AttributeStore->GetCount(pcItems);
		}
		STDMETHODIMP GetItemByIndex(UINT32 unIndex, __RPC__out GUID* pguidKey, __RPC__inout_opt PROPVARIANT* pValue)
		{
			return this->_AttributeStore->GetItemByIndex(unIndex, pguidKey, pValue);
		}
		STDMETHODIMP CopyAllItems(__RPC__in_opt IMFAttributes* pDest)
		{
			return this->_AttributeStore->CopyAllItems(pDest);
		}

		// ヘルパメソッド

		// dwOptions ...  MF_ATTRIBUTE_SERIALIZE_OPTIONSフラグ
		HRESULT SerializeToStream(DWORD dwOptions, IStream* pStm)
		{
			return ::MFSerializeAttributesToStream(this->_AttributeStore, dwOptions, pStm);
		}

		// dwOptions ...  MF_ATTRIBUTE_SERIALIZE_OPTIONSフラグ
		HRESULT DeserializeFromStream(DWORD dwOptions, IStream* pStm)
		{
			return ::MFDeserializeAttributesFromStream(this->_AttributeStore, dwOptions, pStm);
		}

		// 属性ストアをバイト配列に格納する。
		//   ppBuf		…… バイト配列を受け取るポインタ。利用が終わったら、CoTaskMemFree() で解放すること。
		//   pcbSize	…… バイト配列のサイズを受け取るポインタ。
		HRESULT SerializeToBlob(UINT8** ppBuffer, UINT* pcbSize)
		{
			if (ppBuffer == NULL)
				return E_POINTER;

			if (pcbSize == NULL)
				return E_POINTER;

			HRESULT hr = S_OK;
			UINT32 cbSize = 0;
			BYTE* pBuffer = NULL;

			if (FAILED(hr = ::MFGetAttributesAsBlobSize(this->_AttributeStore, &cbSize)))
				return hr;

			pBuffer = (BYTE*) ::CoTaskMemAlloc(cbSize);
			if (pBuffer == NULL)
			{
				return E_OUTOFMEMORY;
			}

			if (FAILED(hr = ::MFGetAttributesAsBlob(this->_AttributeStore, pBuffer, cbSize)))
				return hr;

			*ppBuffer = pBuffer;
			*pcbSize = cbSize;

			if (FAILED(hr))
			{
				*ppBuffer = NULL;
				*pcbSize = 0;
				::CoTaskMemFree(pBuffer);
			}

			return hr;
		}
		HRESULT DeserializeFromBlob(const UINT8* pBuffer, UINT cbSize)
		{
			return ::MFInitAttributesFromBlob(this->_AttributeStore, pBuffer, cbSize);
		}

		HRESULT GetRatio(REFGUID guidKey, UINT32* pnNumerator, UINT32* punDenominator)
		{
			return ::MFGetAttributeRatio(this->_AttributeStore, guidKey, pnNumerator, punDenominator);
		}
		HRESULT SetRatio(REFGUID guidKey, UINT32 unNumerator, UINT32 unDenominator)
		{
			return ::MFSetAttributeRatio(this->_AttributeStore, guidKey, unNumerator, unDenominator);
		}

		// サイズ（幅、高さ）を表す属性を取得する。
		HRESULT GetSize(REFGUID guidKey, UINT32* punWidth, UINT32* punHeight)
		{
			return ::MFGetAttributeSize(this->_AttributeStore, guidKey, punWidth, punHeight);
		}

		// サイズ（幅、高さ）を洗わず属性を設定する。
		HRESULT SetSize(REFGUID guidKey, UINT32 unWidth, UINT32 unHeight)
		{
			return ::MFSetAttributeSize(this->_AttributeStore, guidKey, unWidth, unHeight);
		}

	protected:
		IMFAttributes* _AttributeStore;
	};
}
