#pragma once

namespace D3D11TextureMediaSink
{
	// �N���e�B�J���Z�N�V������wrapper�B
	// �R���X�g���N�^�ŃN���e�B�J���Z�N�V���������������A�f�X�g���N�^�ō폜����B
	class CriticalSection
	{
	public:

		CriticalSection()
		{
			::InitializeCriticalSection(&m_cs);
		}
		~CriticalSection()
		{
			::DeleteCriticalSection(&m_cs);
		}

		_Acquires_lock_(this->m_cs)
		void Lock(void)
		{
			::EnterCriticalSection(&m_cs);
		}

		_Releases_lock_(this->m_cs)
		void Unlock(void)
		{
			::LeaveCriticalSection(&m_cs);
		}

	private:
		CRITICAL_SECTION m_cs;
	};
}
