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
		HRESULT SetClockRate(float 再生レート);

		HRESULT Start(IMFClock* pClock);
		HRESULT Stop();
		HRESULT Flush();

		HRESULT ScheduleSample(IMFSample* pSample, BOOL bPresentNow);

	private:
		const int SCHEDULER_TIMEOUT = 5000; // 5秒
		const int _INFINITE = -1;

		CriticalSection* _csStreamSinkAndScheduler;
		SchedulerCallback*  _PresentCallback = NULL;
		MFTIME _フレーム間隔;
		LONGLONG _フレーム間隔の四分の一;		// よく使うので事前に計算しておく。
		float _再生レート = 1.0f;
		IMFClock* _PresentationClock = NULL;	// NULL 設定可。

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
		ThreadSafeComPtrQueue<IMFSample>*  _表示待ちサンプルキュー;
		BOOL _スレッド実行中 = FALSE;
		PTP_WORK _スレッドハンドル = NULL;
		HANDLE _スレッド起動完了通知;
		ThreadSafePtrQueue<ScheduleEvent>* _ScheduleEventQueue;
		HANDLE _MsgEvent = NULL;
		HANDLE _Flush完了通知 = NULL;

		static void CALLBACK SchedulerThreadProcProxy(PTP_CALLBACK_INSTANCE pInstance, void* arg, PTP_WORK pWork);
		UINT SchedulerThreadProcPrivate();
		HRESULT ProcessSamplesInQueue(DWORD* plNextSleep);
		HRESULT ProcessSample(IMFSample* pSample, DWORD* plNextSleep);
		int MFTimeToMsec(LONGLONG time);
		HRESULT PresentSample(IMFSample* pSample);
	};
}
