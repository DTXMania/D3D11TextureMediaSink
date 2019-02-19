#pragma once

// {FD13125A-546C-4271-98DC-CA2058682AC7}
// IMFSample �ւ̃J�X�^������(uint)
DEFINE_GUID(SAMPLE_STATE, 0xfd13125a, 0x546c, 0x4271, 0x98, 0xdc, 0xca, 0x20, 0x58, 0x68, 0x2a, 0xc7);
#define SAMPLE_STATE_READY		0	// ���g�p
#define SAMPLE_STATE_UPDATING	1	// �X�V��
#define SAMPLE_STATE_SCHEDULED	2	// �X�P�W���[����
#define SAMPLE_STATE_PRESENT	3	// �\����

#define SAMPLE_MAX		5

namespace D3D11TextureMediaSink
{
	class SampleAllocator
	{
	public:
		SampleAllocator();
		~SampleAllocator();

		HRESULT Initialize(ID3D11Device* pD3DDevice, int width, int height);
		HRESULT Shutdown();
		HRESULT GetSample(IMFSample** ppSample);
		HRESULT ReleaseSample(IMFSample* pSample);

	private:
		BOOL _Shutdown�ς� = TRUE;
		IMFSample* _SampleQueue[SAMPLE_MAX];
		CriticalSection _csSampleAllocator;
		HANDLE _�󂫂��ł���;
		HRESULT CheckShutdown();
	};
}
