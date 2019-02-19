#pragma once

namespace D3D11TextureMediaSink
{
	struct SchedulerCallback
	{
		virtual HRESULT PresentFrame(IMFSample* pSample) = 0;
	};

	class Scheduler
	{
	public:
		Scheduler(CriticalSection* critSec);
		~Scheduler();

		void SetCallback(SchedulerCallback* pCB)
		{
			this->_PresentCallback = pCB;
		}
		HRESULT SetFrameRate(const MFRatio& fps);
		HRESULT SetClockRate(float �Đ����[�g);

		HRESULT Start(IMFClock* pClock);
		HRESULT Stop();
		HRESULT Flush();

		HRESULT ScheduleSample(IMFSample* pSample, BOOL bPresentNow);

	private:
		const int SCHEDULER_TIMEOUT = 5000; // 5�b
		const int _INFINITE = -1;

		CriticalSection* _csStreamSinkAndScheduler;
		SchedulerCallback*  _PresentCallback = NULL;
		MFTIME _�t���[���Ԋu;
		LONGLONG _�t���[���Ԋu�̎l���̈�;		// �悭�g���̂Ŏ��O�Ɍv�Z���Ă����B
		float _�Đ����[�g = 1.0f;
		IMFClock* _PresentationClock = NULL;	// NULL �ݒ�B

		enum ScheduleEventType
		{
			eTerminate,
			eSchedule,
			eFlush,
		};
		class ScheduleEvent
		{
		public:
			ScheduleEventType Type;
			ScheduleEvent(ScheduleEventType type)
			{
				this->Type = type;
			}
		};
		ThreadSafeComPtrQueue<IMFSample>*  _�\���҂��T���v���L���[;
		BOOL _�X���b�h���s�� = FALSE;
		PTP_WORK _�X���b�h�n���h�� = NULL;
		HANDLE _�X���b�h�N�������ʒm;
		ThreadSafePtrQueue<ScheduleEvent>* _ScheduleEventQueue;
		HANDLE _MsgEvent = NULL;
		HANDLE _Flush�����ʒm = NULL;

		static void CALLBACK SchedulerThreadProcProxy(PTP_CALLBACK_INSTANCE pInstance, void* arg, PTP_WORK pWork);
		UINT SchedulerThreadProcPrivate();
		HRESULT ProcessSamplesInQueue(DWORD* plNextSleep);
		HRESULT ProcessSample(IMFSample* pSample, DWORD* plNextSleep);
		int MFTimeToMsec(LONGLONG time);
		HRESULT PresentSample(IMFSample* pSample);
	};
}
