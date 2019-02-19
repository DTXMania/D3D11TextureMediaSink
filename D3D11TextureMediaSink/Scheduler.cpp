#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	Scheduler::Scheduler(CriticalSection* critSec)
	{
		this->_�\���҂��T���v���L���[ = new ThreadSafeComPtrQueue<IMFSample>();
		this->_ScheduleEventQueue = new ThreadSafePtrQueue<ScheduleEvent>();
		this->_csStreamSinkAndScheduler = critSec;
		this->_�t���[���Ԋu = 0;
		this->_�t���[���Ԋu�̎l���̈� = 0;

		this->_MsgEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		this->_Flush�����ʒm = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	Scheduler::~Scheduler()
	{
		this->Stop();
	}

	HRESULT Scheduler::SetFrameRate(const MFRatio& fps)
	{
		HRESULT hr;

		// ������ MFRatio �� MFTIME �ɕϊ����ă����o�ɕۑ��B
		UINT64 AvgTimePerFrame = 0;
		if (FAILED(hr = ::MFFrameRateToAverageTimePerFrame(fps.Numerator, fps.Denominator, &AvgTimePerFrame)))
			return hr;

		this->_�t���[���Ԋu = (MFTIME)AvgTimePerFrame;
		this->_�t���[���Ԋu�̎l���̈� = this->_�t���[���Ԋu / 4;	// �Z�o���� MFTIME �� 1/4 ���A�悭�g���̂Ŏ��O�Ɍv�Z���Ă����B

		return S_OK;
	}
	HRESULT Scheduler::SetClockRate(float �Đ����[�g)
	{
		this->_�Đ����[�g = �Đ����[�g;

		return S_OK;
	}

	HRESULT Scheduler::Start(IMFClock* pClock)
	{
		HRESULT hr = S_OK;

		// �v���[���e�[�V�����N���b�N���X�V����B
		SafeRelease(this->_PresentationClock);
		this->_PresentationClock = pClock;
		if (NULL != this->_PresentationClock)
			this->_PresentationClock->AddRef();

		// �X�P�W���[���X���b�h�N��
		this->_�X���b�h�N�������ʒm = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		this->_�X���b�h�n���h�� = ::CreateThreadpoolWork(&Scheduler::SchedulerThreadProcProxy, this, NULL);	// ���[�J�X���b�h���m��
		::SubmitThreadpoolWork(this->_�X���b�h�n���h��);	// ���[�J�X���b�h���P�N��
		DWORD r = ::WaitForSingleObject(this->_�X���b�h�N�������ʒm, SCHEDULER_TIMEOUT);	// �N�������҂�
		this->_�X���b�h���s�� = true;
		::CloseHandle(this->_�X���b�h�N�������ʒm);

		return S_OK;
	}
	HRESULT Scheduler::Stop()
	{
		if (this->_�X���b�h���s�� == FALSE)
			return S_FALSE; // �N�����Ă��Ȃ�

		// �I���C�x���g���Z�b�g���ăX���b�h�֒ʒm�B
		this->_ScheduleEventQueue->Queue(new ScheduleEvent(eTerminate));
		::SetEvent(this->_MsgEvent);
		::WaitForThreadpoolWorkCallbacks(this->_�X���b�h�n���h��, FALSE);	// ���[�J�X���b�h�I���܂ő҂�
		this->_�X���b�h���s�� = FALSE;

		// �L���[�̃T���v�������ׂĔj������B
		this->_�\���҂��T���v���L���[->Clear();

		// �v���[���e�[�V�����N���b�N������B
		SafeRelease(this->_PresentationClock);

		return S_OK;
	}
	HRESULT Scheduler::Flush()
	{
		AutoLock lock(this->_csStreamSinkAndScheduler);

		if (this->_�X���b�h���s��)
		{
			// �t���b�V���C�x���g���Z�b�g���ăX���b�h�֒ʒm�B
			this->_ScheduleEventQueue->Queue(new ScheduleEvent(eFlush));
			::SetEvent(this->_MsgEvent);

			// �t���b�V���̊����҂��B
			DWORD r = ::WaitForSingleObject(this->_Flush�����ʒm, SCHEDULER_TIMEOUT);
			DWORD s = r;
		}

		return S_OK;
	}

	HRESULT Scheduler::ScheduleSample(IMFSample* pSample, BOOL bPresentNow)
	{
		if (NULL == this->_PresentCallback)
			return MF_E_NOT_INITIALIZED;

		HRESULT hr = S_OK;

		if (bPresentNow || (NULL == this->_PresentationClock))
		{
			// (A) �T���v���������ɕ\������B

			if (FAILED(hr = this->PresentSample(pSample)))
				return hr;
		}
		else
		{
			// (B) �T���v����\���҂��L���[�Ɋi�[���A�X�P�W���[���X���b�h�ɒʒm�B

			if (FAILED(hr = pSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_SCHEDULED)))
				return hr;

			this->_�\���҂��T���v���L���[->Queue(pSample);

			//TCHAR buf[1024];
			//wsprintf(buf, L"Scheduler::ScheduleSample - queued.(%X)\n", pSample);
			//OutputDebugString(buf);

			this->_ScheduleEventQueue->Queue(new ScheduleEvent(eSchedule));
			::SetEvent(this->_MsgEvent);
		}

		return S_OK;
	}

	void CALLBACK Scheduler::SchedulerThreadProcProxy(PTP_CALLBACK_INSTANCE pInstance, LPVOID arg, PTP_WORK pWork)
	{
		// static ����C���X�^���X��
		reinterpret_cast<Scheduler*>(arg)->SchedulerThreadProcPrivate();
	}
	UINT Scheduler::SchedulerThreadProcPrivate()
	{
		DWORD taskIndex = 0;
		::AvSetMmThreadCharacteristics(TEXT("Playback"), &taskIndex);

		// �X���b�h�N�������ʒm�𑗂�B
		::SetEvent(this->_�X���b�h�N�������ʒm);

		//OutputDebugString(L"�X���b�h�N�������B\n");

		DWORD lWait = INFINITE;
		BOOL bExitThread = FALSE;

		while (bExitThread == FALSE)
		{
			// (A)lWait���^�C���A�E�g���邩�A(B)�C�x���g���L���b�`����܂ő҂B

			//_OutputDebugString(L"Scheduler::Thread, lWait = %d ms �� wait ���܂��B\n", lWait);

			::WaitForSingleObject(this->_MsgEvent, lWait);


			if (0 == this->_ScheduleEventQueue->GetCount() && lWait != INFINITE)
			{
				//OutputDebugString(L"Scheduler::Thread, �^�C���A�E�g�Ŗڊo�߂܂����B\n");

				// (A)�^�C���A�E�g�����ꍇ �� �T���v���̕\�������ł���
				if (FAILED(this->ProcessSamplesInQueue(&lWait)))    // ���̑ҋ@���Ԃ��󂯎��
				{
					bExitThread = TRUE; // �G���[����
					break;
				}
			}
			else
			{
				// (B) �C�x���g���L���b�`�����ꍇ �� �C�x���g����������

				BOOL bProcessSamples = TRUE;
				while (0 < this->_ScheduleEventQueue->GetCount())
				{
					ScheduleEvent* pEvent;
					if (FAILED(this->_ScheduleEventQueue->Dequeue(&pEvent)))
						break;	// �O�̂���

					switch (pEvent->Type)
					{
					case eTerminate:
						//OutputDebugString(L"Scheduler::Thread, �C�x���g eTerminate �Ŗڊo�߂܂����B\n");
						bExitThread = TRUE;
						break;

					case eFlush:
						//OutputDebugString(L"Scheduler::Thread, �C�x���g eFlush �Ŗڊo�߂܂����B\n");
						this->_�\���҂��T���v���L���[->Clear();	// �L���[���t���b�V��
						lWait = INFINITE;
						::SetEvent(this->_Flush�����ʒm); // �t���b�V�������ʒm
						break;

					case eSchedule:
						//OutputDebugString(L"Scheduler::Thread, �C�x���g eSchedule �Ŗڊo�߂܂����B\n");
						if (bProcessSamples)
						{
							if (FAILED(this->ProcessSamplesInQueue(&lWait)))    // �T���v�����������A���̑ҋ@���Ԃ��󂯎��
							{
								bExitThread = TRUE;
								break;
							}
							bProcessSamples = (lWait != INFINITE);
						}
						break;
					}
				}
			}
		}

		//OutputDebugString(L"�X���b�h�I���B\n");
		return 0;
	}
	HRESULT Scheduler::ProcessSamplesInQueue(DWORD* plNextSleep)
	{
		HRESULT hr = S_OK;

		if (NULL == plNextSleep)
			return E_POINTER;

		*plNextSleep = 0;

		while (0 < this->_�\���҂��T���v���L���[->GetCount())
		{
			// �L���[����T���v�����擾����B
			IMFSample* pSample;
			if (FAILED(hr = this->_�\���҂��T���v���L���[->Dequeue(&pSample)))
				return hr;

			// �T���v���̕\������B
			if (FAILED(hr = this->ProcessSample(pSample, plNextSleep)))
				return hr;

			if (0 < *plNextSleep)   // �܂��\����������Ȃ�������A
			{
				this->_�\���҂��T���v���L���[->PutBack(pSample);  // �L���[�ɍ����߂���
				break;      // ���[�v���I���B
			}
		}

		if (0 == *plNextSleep)
			*plNextSleep = INFINITE;

		return hr;
	}
	HRESULT Scheduler::ProcessSample(IMFSample* pSample, DWORD* plNextSleep)
	{
		HRESULT hr = S_OK;
		*plNextSleep = 0;
		bool bPresentNow = true;

		if (NULL != this->_PresentationClock)   // �N���b�N������
		{
			// �T���v���̕\���������擾�B
			LONGLONG hnsPresentationTime;
			if (FAILED(hr = pSample->GetSampleTime(&hnsPresentationTime)))
				hnsPresentationTime = 0;    // �^�C���X�^���v���Ȃ��Ă�����

			// �N���b�N���猻�ݎ������擾�B
			LONGLONG hnsTimeNow;
			MFTIME hnsSystemTime;
			if (FAILED(hr = this->_PresentationClock->GetCorrelatedTime(0, &hnsTimeNow, &hnsSystemTime)))
				return hr;

			// �T���v���̕\�������܂ł̎��Ԃ��v�Z����B�����́A�T���v�����x��Ă��邱�Ƃ��Ӗ�����B
			LONGLONG hnsDelta = hnsPresentationTime - hnsTimeNow;
			if (0 > this->_�Đ����[�g)
			{
				// �t�Đ����ɂ́A�N���b�N���t������B���̂��߁A���Ԃ����]����B
				hnsDelta = -hnsDelta;
			}

			// �T���v���̕\������� plNextSleep �̎Z�o
			if (hnsDelta < -this->_�t���[���Ԋu�̎l���̈�)
			{
				// �T���v���͒x��Ă���̂ł����\������B
				bPresentNow = true;
			}
			else if ((3 * this->_�t���[���Ԋu�̎l���̈�) < hnsDelta)
			{
				// ���̃T���v���̕\���͂܂������B
				*plNextSleep = this->MFTimeToMsec(hnsDelta - (3 * this->_�t���[���Ԋu�̎l���̈�));
				*plNextSleep = ::abs((int)(*plNextSleep / this->_�Đ����[�g));   // �Đ����[�g�𔽉f����B

				// �܂��\�����Ȃ��B
				bPresentNow = false;

				if (*plNextSleep == 0)	// ���傤�� 0 ���� INFINITE �����ɂȂ��Ă��܂��̂ŉ���B
					*plNextSleep = 1;

				//TCHAR buf[1024];
				//wsprintf(buf, L"Scheduler::ProcessSamples: �T���v���͂܂��\�����܂���B(%x)(lWait=%d)\n", pSample, *plNextSleep);
				//OutputDebugString(buf);
			}
		}

		// �\������B
		if (bPresentNow)
		{
			this->PresentSample(pSample);

			//TCHAR buf[1024];
			//wsprintf(buf, L"Scheduler::ProcessSamples: �T���v���͕\������܂����B(%x)\n", pSample);
			//OutputDebugString(buf);
		}

		return S_OK;
	}
	int Scheduler::MFTimeToMsec(LONGLONG time)
	{
		const LONGLONG ONE_SECOND = 10000000; // One second in hns
		const int ONE_MSEC = 1000;       // One msec in hns

		return (int)(time / (ONE_SECOND / ONE_MSEC));
	}
	HRESULT Scheduler::PresentSample(IMFSample* pSample)
	{
		HRESULT hr = S_OK;

		// �R�[���o�b�N�Ăяo���B
		this->_PresentCallback->PresentFrame(pSample);

		return S_OK;
	}
}
