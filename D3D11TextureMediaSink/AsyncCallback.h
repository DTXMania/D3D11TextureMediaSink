#pragma once

namespace D3D11TextureMediaSink
{
	// AsyncCallback [テンプレート付きクラス]
	//
	//  IMFAsyncCallback を実装するクラス。IMFAsyncCallback::Invoke 呼び出しを、
	//  指定したインスタンスの指定したメソッドにルーティングする。
	//  １つのクラスで複数の IMFAsyncCallback を実装したい場合に使う。
	//
	//	使用例:
	//
	//	class CTest : public IUnknown
	//	{
	//	public:
	//		CTest() :
	//			m_cb1(this, &CTest::OnInvoke1),		// m_cb1::Invoke は this->OnInvoke1() を呼び出す。
	//			m_cb2(this, &CTest::OnInvoke2)		// m_cb2::Invoke は this->OnInvoke2() を呼び出す。
	//		{ }
	//		STDMETHODIMP_(ULONG) AddRef();
	//		STDMETHODIMP_(ULONG) Release();
	//		STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv);
	//
	//	private:
	//		AsyncCallback<CTest> m_cb1;
	//		AsyncCallback<CTest> m_cb2;
	//		void OnInvoke1(IMFAsyncResult* pAsyncResult);
	//		void OnInvoke2(IMFAsyncResult* pAsyncResult);
	//	}
	//
	template<class T>	// T: 親オブジェクトの型。親オブジェクトは COM であり、かつ InvokeFn 型のメンバを提供すること。
	class AsyncCallback : public IMFAsyncCallback
	{
	public:
		typedef HRESULT(T::*InvokeFn)(IMFAsyncResult* pAsyncResult);

		AsyncCallback(T* pParent, InvokeFn fn)
		{
			this->m_pParent = pParent;
			this->m_pInvokeFn = fn;
		}
		~AsyncCallback()
		{
			this->m_pInvokeFn = nullptr;
			this->m_pParent = nullptr;
		}

		// IUnknown 実装
		STDMETHODIMP_(ULONG) AddRef()
		{
			return this->m_pParent->AddRef();	// 親へ移譲。
		}
		STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
		{
			if (!ppv)
				return E_POINTER;

			if (iid == __uuidof(IUnknown))
			{
				*ppv = static_cast<IUnknown*>(static_cast<IMFAsyncCallback*>(this));
			}
			else if (iid == __uuidof(IMFAsyncCallback))
			{
				*ppv = static_cast<IMFAsyncCallback*>(this);
			}
			else
			{
				*ppv = nullptr;
				return E_NOINTERFACE;
			}

			this->AddRef();

			return S_OK;
		}
		STDMETHODIMP_(ULONG) Release()
		{
			return this->m_pParent->Release();	// 親へ移譲。
		}

		// IMFAsyncCallback 実装
		STDMETHODIMP GetParameters(__RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue)
		{
			// このメソッドの実装はオプション。
			return E_NOTIMPL;
		}
		STDMETHODIMP Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult)
		{
			// コールバックを呼び出す。
			return (this->m_pParent->*m_pInvokeFn)(pAsyncResult);
		}

	private:
		T* m_pParent;
		InvokeFn m_pInvokeFn;
	};
}
