#include "stdafx.h"

namespace D3D11TextureMediaSink
{
	Scheduler::Scheduler(CriticalSection* critSec)
	{
		this->_表示待ちサンプルキュー = new ThreadSafeComPtrQueue<IMFSample>();
		this->_ScheduleEventQueue = new ThreadSafePtrQueue<ScheduleEvent>();
		this->_csStreamSinkAndScheduler = critSec;
		this->_フレーム間隔 = 0;
		this->_フレーム間隔の四分の一 = 0;

		this->_MsgEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		this->_Flush完了通知 = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	Scheduler::~Scheduler()
	{
		this->Stop();
	}

	HRESULT Scheduler::SetFrameRate(const MFRatio& fps)
	{
		HRESULT hr;

		// 引数の MFRatio を MFTIME に変換してメンバに保存。
		UINT64 AvgTimePerFrame = 0;
		if (FAILED(hr = ::MFFrameRateToAverageTimePerFrame(fps.Numerator, fps.Denominator, &AvgTimePerFrame)))
			return hr;

		this->_フレーム間隔 = (MFTIME)AvgTimePerFrame;
		this->_フレーム間隔の四分の一 = this->_フレーム間隔 / 4;	// 算出した MFTIME の 1/4 を、よく使うので事前に計算しておく。

		return S_OK;
	}
	HRESULT Scheduler::SetClockRate(float 再生レート)
	{
		this->_再生レート = 再生レート;

		return S_OK;
	}

	HRESULT Scheduler::Start(IMFClock* pClock)
	{
		HRESULT hr = S_OK;

		// プレゼンテーションクロックを更新する。
		SafeRelease(this->_PresentationClock);
		this->_PresentationClock = pClock;
		if (NULL != this->_PresentationClock)
			this->_PresentationClock->AddRef();

		// スケジューラスレッド起動
		this->_スレッド起動完了通知 = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		this->_スレッドハンドル = ::CreateThreadpoolWork(&Scheduler::SchedulerThreadProcProxy, this, NULL);	// ワーカスレッドを確保
		::SubmitThreadpoolWork(this->_スレッドハンドル);	// ワーカスレッドを１つ起動
		DWORD r = ::WaitForSingleObject(this->_スレッド起動完了通知, SCHEDULER_TIMEOUT);	// 起動完了待ち
		this->_スレッド実行中 = true;
		::CloseHandle(this->_スレッド起動完了通知);

		return S_OK;
	}
	HRESULT Scheduler::Stop()
	{
		if (this->_スレッド実行中 == FALSE)
			return S_FALSE; // 起動していない

		// 終了イベントをセットしてスレッドへ通知。
		this->_ScheduleEventQueue->Queue(new ScheduleEvent(eTerminate));
		::SetEvent(this->_MsgEvent);
		::WaitForThreadpoolWorkCallbacks(this->_スレッドハンドル, FALSE);	// ワーカスレッド終了まで待つ
		this->_スレッド実行中 = FALSE;

		// キューのサンプルをすべて破棄する。
		this->_表示待ちサンプルキュー->Clear();

		// プレゼンテーションクロックを解放。
		SafeRelease(this->_PresentationClock);

		return S_OK;
	}
	HRESULT Scheduler::Flush()
	{
		AutoLock lock(this->_csStreamSinkAndScheduler);

		if (this->_スレッド実行中)
		{
			// フラッシュイベントをセットしてスレッドへ通知。
			this->_ScheduleEventQueue->Queue(new ScheduleEvent(eFlush));
			::SetEvent(this->_MsgEvent);

			// フラッシュの完了待ち。
			DWORD r = ::WaitForSingleObject(this->_Flush完了通知, SCHEDULER_TIMEOUT);
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
			// (A) サンプルをすぐに表示する。

			if (FAILED(hr = this->PresentSample(pSample)))
				return hr;
		}
		else
		{
			// (B) サンプルを表示待ちキューに格納し、スケジューラスレッドに通知。

			if (FAILED(hr = pSample->SetUINT32(SAMPLE_STATE, SAMPLE_STATE_SCHEDULED)))
				return hr;

			this->_表示待ちサンプルキュー->Queue(pSample);

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
		// static からインスタンスへ
		reinterpret_cast<Scheduler*>(arg)->SchedulerThreadProcPrivate();
	}
	UINT Scheduler::SchedulerThreadProcPrivate()
	{
		DWORD taskIndex = 0;
		::AvSetMmThreadCharacteristics(TEXT("Playback"), &taskIndex);

		// スレッド起動完了通知を送る。
		::SetEvent(this->_スレッド起動完了通知);

		//OutputDebugString(L"スレッド起動完了。\n");

		DWORD lWait = INFINITE;
		BOOL bExitThread = FALSE;

		while (bExitThread == FALSE)
		{
			// (A)lWaitがタイムアウトするか、(B)イベントをキャッチするまで待つ。

			//_OutputDebugString(L"Scheduler::Thread, lWait = %d ms で wait します。\n", lWait);

			::WaitForSingleObject(this->_MsgEvent, lWait);


			if (0 == this->_ScheduleEventQueue->GetCount() && lWait != INFINITE)
			{
				//OutputDebugString(L"Scheduler::Thread, タイムアウトで目覚めました。\n");

				// (A)タイムアウトした場合 → サンプルの表示時刻である
				if (FAILED(this->ProcessSamplesInQueue(&lWait)))    // 次の待機時間を受け取る
				{
					bExitThread = TRUE; // エラー発生
					break;
				}
			}
			else
			{
				// (B) イベントをキャッチした場合 → イベントを処理する

				BOOL bProcessSamples = TRUE;
				while (0 < this->_ScheduleEventQueue->GetCount())
				{
					ScheduleEvent* pEvent;
					if (FAILED(this->_ScheduleEventQueue->Dequeue(&pEvent)))
						break;	// 念のため

					switch (pEvent->Type)
					{
					case eTerminate:
						//OutputDebugString(L"Scheduler::Thread, イベント eTerminate で目覚めました。\n");
						bExitThread = TRUE;
						break;

					case eFlush:
						//OutputDebugString(L"Scheduler::Thread, イベント eFlush で目覚めました。\n");
						this->_表示待ちサンプルキュー->Clear();	// キューをフラッシュ
						lWait = INFINITE;
						::SetEvent(this->_Flush完了通知); // フラッシュ完了通知
						break;

					case eSchedule:
						//OutputDebugString(L"Scheduler::Thread, イベント eSchedule で目覚めました。\n");
						if (bProcessSamples)
						{
							if (FAILED(this->ProcessSamplesInQueue(&lWait)))    // サンプルを処理し、次の待機時間を受け取る
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

		//OutputDebugString(L"スレッド終了。\n");
		return 0;
	}
	HRESULT Scheduler::ProcessSamplesInQueue(DWORD* plNextSleep)
	{
		HRESULT hr = S_OK;

		if (NULL == plNextSleep)
			return E_POINTER;

		*plNextSleep = 0;

		while (0 < this->_表示待ちサンプルキュー->GetCount())
		{
			// キューからサンプルを取得する。
			IMFSample* pSample;
			if (FAILED(hr = this->_表示待ちサンプルキュー->Dequeue(&pSample)))
				return hr;

			// サンプルの表示判定。
			if (FAILED(hr = this->ProcessSample(pSample, plNextSleep)))
				return hr;

			if (0 < *plNextSleep)   // まだ表示時刻じゃなかったら、
			{
				this->_表示待ちサンプルキュー->PutBack(pSample);  // キューに差し戻して
				break;      // ループを終了。
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

		if (NULL != this->_PresentationClock)   // クロックがある
		{
			// サンプルの表示時刻を取得。
			LONGLONG hnsPresentationTime;
			if (FAILED(hr = pSample->GetSampleTime(&hnsPresentationTime)))
				hnsPresentationTime = 0;    // タイムスタンプがなくても正常

			// クロックから現在時刻を取得。
			LONGLONG hnsTimeNow;
			MFTIME hnsSystemTime;
			if (FAILED(hr = this->_PresentationClock->GetCorrelatedTime(0, &hnsTimeNow, &hnsSystemTime)))
				return hr;

			// サンプルの表示時刻までの時間を計算する。負数は、サンプルが遅れていることを意味する。
			LONGLONG hnsDelta = hnsPresentationTime - hnsTimeNow;
			if (0 > this->_再生レート)
			{
				// 逆再生時には、クロックも逆走する。そのため、時間も反転する。
				hnsDelta = -hnsDelta;
			}

			// サンプルの表示判定と plNextSleep の算出
			if (hnsDelta < -this->_フレーム間隔の四分の一)
			{
				// サンプルは遅れているのですぐ表示する。
				bPresentNow = true;
			}
			else if ((3 * this->_フレーム間隔の四分の一) < hnsDelta)
			{
				// このサンプルの表示はまだ早い。
				*plNextSleep = this->MFTimeToMsec(hnsDelta - (3 * this->_フレーム間隔の四分の一));
				*plNextSleep = ::abs((int)(*plNextSleep / this->_再生レート));   // 再生レートを反映する。

				// まだ表示しない。
				bPresentNow = false;

				if (*plNextSleep == 0)	// ちょうど 0 だと INFINITE 扱いになってしまうので回避。
					*plNextSleep = 1;

				//TCHAR buf[1024];
				//wsprintf(buf, L"Scheduler::ProcessSamples: サンプルはまだ表示しません。(%x)(lWait=%d)\n", pSample, *plNextSleep);
				//OutputDebugString(buf);
			}
		}

		// 表示する。
		if (bPresentNow)
		{
			this->PresentSample(pSample);

			//TCHAR buf[1024];
			//wsprintf(buf, L"Scheduler::ProcessSamples: サンプルは表示されました。(%x)\n", pSample);
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

		// コールバック呼び出し。
		this->_PresentCallback->PresentFrame(pSample);

		return S_OK;
	}
}
