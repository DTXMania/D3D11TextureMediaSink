#pragma once

namespace D3D11TextureMediaSink
{
	// IMarker:
	// IMFStreamSink::PlaceMarker �Ăяo����񓯊��Ƀn���h�����O���邽�߂̃J�X�^���C���^�[�t�F�[�X�B
	//
	// �}�[�J�[�́A
	// �@�E�}�[�J�[�^		(MFSTREAMSINK_MARKER_TYPE)
	// �@�E�}�[�J�[�f�[�^	(PROVARIANT)
	// �@�E�R���e�L�X�g		(PROVARIANT)
	// �ō\�������B
	//
	// ���̃C���^�[�t�F�[�X�ɂ��A�}�[�J�[�f�[�^��IUnknown�I�u�W�F�N�g�̓����Ɋi�[���邱�Ƃ��ł��A
	// ���̃I�u�W�F�N�g���A���̃��f�B�A�^�C�v��ێ����Ă���L���[�Ɠ����L���[�Ɉێ����邱�Ƃ��ł���B
	// ����́A�T���v���ƃ}�[�J�[�̓V���A���C�Y����Ȃ���΂Ȃ�Ȃ��Ƃ������R�ɂ��A�֗��ł���B
	// �܂�A���̑O�ɗ������ׂẴT���v�����������I���܂ŁA�}�[�J�[�ɂ��ĐӔC�������Ƃ��ł��Ȃ��B
	//
	// IMarker �͕W�� Media Foundation �C���^�[�t�F�[�X�ł͂Ȃ��̂Œ��ӂ��邱�ƁB
	//
	MIDL_INTERFACE("3AC82233-933C-43a9-AF3D-ADC94EABF406")
	IMarker : public IUnknown
	{
		virtual STDMETHODIMP GetType(MFSTREAMSINK_MARKER_TYPE* pType) = 0;
		virtual STDMETHODIMP GetValue(PROPVARIANT* pvar) = 0;
		virtual STDMETHODIMP GetContext(PROPVARIANT* pvar) = 0;
	};
}
