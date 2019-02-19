#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	Marker::Marker(MFSTREAMSINK_MARKER_TYPE �^)
	{
		this->�Q�ƃJ�E���^ = 1;
		this->�^ = �^;
		::PropVariantInit(&this->�l);
		::PropVariantInit(&this->�R���e�L�X�g);
	}
	Marker::~Marker()
	{
		::PropVariantClear(&this->�l);
		::PropVariantClear(&this->�R���e�L�X�g);
	}

	// static

	HRESULT Marker::Create(
		MFSTREAMSINK_MARKER_TYPE	�^,
		const PROPVARIANT*			�l,			// NULL �ɂł���B
		const PROPVARIANT*			�R���e�L�X�g,	// NULL �ɂł���B
		IMarker**					ppMarker	// [out] �쐬���� IMarker ���󂯎��|�C���^�B
	)
	{
		if (NULL == ppMarker)
			return E_POINTER;

		HRESULT hr = S_OK;

		Marker* pMarker = new Marker(�^);
		do
		{
			// �w�肳�ꂽ�^�������āAMarker ��V�K�����B
			if (NULL == pMarker)
			{
				hr = E_OUTOFMEMORY;
				break;
			}

			// �w�肳�ꂽ�l��Marker�̒l�ɃR�s�[�B
			if (NULL != �l)
				if (FAILED(hr = ::PropVariantCopy(&pMarker->�l, �l)))
					break;

			// �w�肳�ꂽ�R���e�L�X�g��CMarker�̃R���e�L�X�g�ɃR�s�[�B
			if (�R���e�L�X�g)
				if (FAILED(hr = ::PropVariantCopy(&pMarker->�R���e�L�X�g, �R���e�L�X�g)))
					break;

			// �߂�l�p�ɎQ�ƃJ�E���^���X�V���āA�����B
			*ppMarker = pMarker;
			(*ppMarker)->AddRef();

		} while (FALSE);

		SafeRelease(pMarker);

		return hr;
	}

	// IUnknown ����
	ULONG Marker::AddRef()
	{
		return InterlockedIncrement(&this->�Q�ƃJ�E���^);
	}
	ULONG Marker::Release()
	{
		// �X���b�h�Z�[�t�ɂ��邽�߂ɁA�e���|�����ϐ��Ɉڂ����l���擾���A�Ԃ��B
		ULONG uCount = InterlockedDecrement(&this->�Q�ƃJ�E���^);

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

	// IMarker ����
	HRESULT Marker::GetType(MFSTREAMSINK_MARKER_TYPE* pType)
	{
		if (pType == NULL)
			return E_POINTER;

		*pType = this->�^;

		return S_OK;
	}
	HRESULT Marker::GetValue(PROPVARIANT* pvar)
	{
		if (pvar == NULL)
			return E_POINTER;

		return ::PropVariantCopy(pvar, &(this->�l));

	}
	HRESULT Marker::GetContext(PROPVARIANT* pvar)
	{
		if (pvar == NULL)
			return E_POINTER;

		return ::PropVariantCopy(pvar, &(this->�R���e�L�X�g));
	}
}
