#pragma once

namespace D3D11TextureMediaSink
{
	// AsyncCallback [�e���v���[�g�t���N���X]
	//
	//  IMFAsyncCallback ����������N���X�BIMFAsyncCallback::Invoke �Ăяo�����A
	//  �w�肵���C���X�^���X�̎w�肵�����\�b�h�Ƀ��[�e�B���O����B
	//  �P�̃N���X�ŕ����� IMFAsyncCallback �������������ꍇ�Ɏg���B
	//
	//	�g�p��:
	//
	//	class CTest : public IUnknown
	//	{
	//	public:
	//		CTest() :
	//			m_cb1(this, &CTest::OnInvoke1),		// m_cb1::Invoke �� this->OnInvoke1() ���Ăяo���B
	//			m_cb2(this, &CTest::OnInvoke2)		// m_cb2::Invoke �� this->OnInvoke2() ���Ăяo���B
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
	template<class T>	// T: �e�I�u�W�F�N�g�̌^�B�e�I�u�W�F�N�g�� COM �ł���A���� InvokeFn �^�̃����o��񋟂��邱�ƁB
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

		// IUnknown ����
		STDMETHODIMP_(ULONG) AddRef()
		{
			return this->m_pParent->AddRef();	// �e�ֈڏ��B
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
			return this->m_pParent->Release();	// �e�ֈڏ��B
		}

		// IMFAsyncCallback ����
		STDMETHODIMP GetParameters(__RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue)
		{
			// ���̃��\�b�h�̎����̓I�v�V�����B
			return E_NOTIMPL;
		}
		STDMETHODIMP Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult)
		{
			// �R�[���o�b�N���Ăяo���B
			return (this->m_pParent->*m_pInvokeFn)(pAsyncResult);
		}

	private:
		T* m_pParent;
		InvokeFn m_pInvokeFn;
	};
}
