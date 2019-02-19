#include "stdafx.h"
#include "DemoPlay.h"

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR    lpCmdLine, _In_ int       nCmdShow)
{
	// ウィンドウクラスを登録する。
	LPCWSTR szWindowClass = L"D3D11TextureMediaSinkDemoClass";
	LPCWSTR szTitle = L"D3D11TextureMediaSinkDemo";
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);
	::RegisterClassExW(&wcex);

	// ウィンドウを作成し、表示する。
	HWND hWnd = ::CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 1280, 720, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)
		return FALSE;

	::ShowWindow(hWnd, nCmdShow);
	::UpdateWindow(hWnd);

	// ファイルを選択する。
	TCHAR szFile[MAX_PATH] = TEXT("");
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFilter = 
		TEXT("Movie (*.mp4;*.avi;*.wmv)\0*.mp4;*.avi;*.wmv\0") 
		TEXT("All(*.*)\0*.*\0\0");
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST;
	if (!::GetOpenFileName(&ofn))
		return FALSE;

	// 動画再生クラスを生成し、再生を開始。
	auto demoPlay = new DemoPlay(hWnd);
	HRESULT hr = demoPlay->Play(szFile);
	if (FAILED(hr))
	{
		::MessageBox(hWnd, _T("MediaSession build error!"), _T("Error"), MB_OK);
		return FALSE;
	}

	// メッセージループ。
	MSG msg;
	while (TRUE)
	{
		while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		// 動画再生クラスを呼び出す。
		if (msg.message == WM_QUIT)
		{
			demoPlay->Dispose();
			break;
		}
		else 
		{
			demoPlay->UpdateAndPresent();
		}
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
