#include <Windows.h>
#include <commctrl.h>
#include "resource.h"

const wchar_t TYPER_WINDOWCLAZZ[] = L"Typer Main Window";

typedef struct
{
	wchar_t filename[MAX_PATH];
	bool bHasOpenFile;
} DOCUMENT_STATE;

typedef struct
{
	DOCUMENT_STATE document;
	HINSTANCE hInstance;
	HWND hWnd;
	HWND hWndEdit;
	HICON hIcon;
	HFONT hFont;
} APP_STATE;

APP_STATE state;

DWORD Typer_ShowIOError(LPCWSTR lpStrFileName)
{
	const DWORD err = GetLastError();

	LPTSTR lpBuffer;
	DWORD dwResult = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr,
		err,
		LANG_SYSTEM_DEFAULT,
		LPTSTR(&lpBuffer),
		256,
		nullptr
	);
	
	wchar_t buffer[512] = {0};
	if (dwResult == 0)
	{
		wsprintfW(buffer, L"Error during I/O of file:\n%s [code: %lu]", lpStrFileName, err);
	} else
	{
		wsprintfW(buffer, L"Error during I/O of file: %s\n%s [code: %lu]", lpStrFileName, lpBuffer, err);
	}
	
	MessageBox(nullptr, buffer, L"I/O error", MB_OK | MB_ICONSTOP | MB_DEFBUTTON1);

	return err;	
}

bool Typer_SaveWithFileName(LPWSTR lpStrFileName)
{

	HANDLE hOpenFile = CreateFile(lpStrFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);

	if (hOpenFile == INVALID_HANDLE_VALUE)
	{
		Typer_ShowIOError(lpStrFileName);
		return false;
	}
	
	HANDLE hProcessHeap = GetProcessHeap();
	if (!hProcessHeap)
	{
		Typer_ShowIOError(lpStrFileName);
		CloseHandle(hOpenFile);
		return false;
	}

	const SIZE_T szBufferSize = sizeof(wchar_t) * 1024 * 1024;
	BYTE* pMem = static_cast<BYTE*>(HeapAlloc(hProcessHeap, HEAP_ZERO_MEMORY, szBufferSize));
	if (!pMem)
	{
		Typer_ShowIOError(lpStrFileName);
		CloseHandle(hOpenFile);
		return false;
	}
	
	BYTE* pos = pMem;

	int length = GetWindowTextA(state.hWndEdit, reinterpret_cast<LPSTR>(pos), szBufferSize);

	while (pos != (pMem + length))
	{
		DWORD numBytesWritten = 0;
		if (!WriteFile(hOpenFile, pos, (pMem + length) - pos - numBytesWritten, &numBytesWritten, nullptr))
		{
			Typer_ShowIOError(lpStrFileName);
			HeapFree(hProcessHeap, 0, pMem);
			CloseHandle(hOpenFile);
			return false;
		}
		
		pos += numBytesWritten;
	}

	CloseHandle(hOpenFile);
	SetWindowTextA(state.hWndEdit, reinterpret_cast<LPCSTR>(pMem));
	HeapFree(hProcessHeap, 0, pMem);

	return true;
}

void Typer_RecordOpenFile(LPWSTR lpOpenFileName)
{
	ZeroMemory(state.document.filename, MAX_PATH);
	wcscpy_s(state.document.filename, lpOpenFileName);
	state.document.bHasOpenFile = true;

	const SIZE_T szBuf = MAX_PATH + 64;
	wchar_t buf[szBuf];
	ZeroMemory(buf, szBuf);
	wcscat_s(buf, L"Typer - ");
	wcscat_s(buf, state.document.filename);
	
	SetWindowTextW(state.hWnd, buf);
}

bool Typer_SaveAs(bool bSaveFileName)
{
	// Get the filename from the user
	wchar_t filename[MAX_PATH] = {0};
	
	OPENFILENAME ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hInstance = state.hInstance;
	ofn.hwndOwner = state.hWnd;
	ofn.lpstrTitle = L"Select a path to save the from Typer";
	ofn.lpstrFilter = L"All Files (*.*)\0*.*\0\0";

	ofn.nMaxFile = sizeof(filename);
	ofn.lpstrFile = filename;

	ofn.Flags = OFN_CREATEPROMPT | OFN_HIDEREADONLY | OFN_NODEREFERENCELINKS | OFN_NONETWORKBUTTON | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT;

	if (!GetSaveFileName(&ofn))
	{
		return true;
	}

	if (Typer_SaveWithFileName(ofn.lpstrFile) && bSaveFileName)
	{
		Typer_RecordOpenFile(ofn.lpstrFile);
	}

	return true;
}

bool Typer_SaveAs()
{
	return Typer_SaveAs(false);
}

bool Typer_Save()
{
	if (state.document.bHasOpenFile)
	{
		return Typer_SaveWithFileName(state.document.filename);
	}
	
	Typer_SaveAs(true);
}

bool Typer_OpenFile()
{
	wchar_t filename[MAX_PATH] = {0};
	
	OPENFILENAME ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hInstance = state.hInstance;
	ofn.hwndOwner = state.hWnd;
	ofn.lpstrTitle = L"Select a file to open in Typer";
	ofn.lpstrFilter = L"All Files (*.*)\0*.*\0\0";

	ofn.nMaxFile = sizeof(filename);
	ofn.lpstrFile = filename;

	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NODEREFERENCELINKS | OFN_NONETWORKBUTTON | OFN_PATHMUSTEXIST;

	if (!GetOpenFileName(&ofn))
	{
		return true;
	}

	LPWSTR lpStrFileName = ofn.lpstrFile;
	HANDLE hOpenFile = CreateFile(lpStrFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

	if (hOpenFile == INVALID_HANDLE_VALUE)
	{
		Typer_ShowIOError(lpStrFileName);
		return false;
	}

	HANDLE hProcessHeap = GetProcessHeap();
	if (!hProcessHeap)
	{
		Typer_ShowIOError(lpStrFileName);
		CloseHandle(hOpenFile);
		return false;
	}

	BYTE* pMem = static_cast<BYTE*>(HeapAlloc(hProcessHeap, HEAP_ZERO_MEMORY, sizeof(wchar_t) * 1024 * 1024));
	if (!pMem)
	{
		Typer_ShowIOError(lpStrFileName);
		CloseHandle(hOpenFile);
		return false;
	}
	
	BYTE* pos = pMem;

	while (true)
	{
		DWORD numBytesRead;
		if (!ReadFile(hOpenFile, pos, 1024, &numBytesRead, nullptr))
		{
			Typer_ShowIOError(lpStrFileName);
			HeapFree(hProcessHeap, 0, pMem);
			CloseHandle(hOpenFile);
			return false;
		}

		pos += numBytesRead;
		if (numBytesRead == 0)
		{
			break;
		}
	}

	CloseHandle(hOpenFile);
	SetWindowTextA(state.hWndEdit, reinterpret_cast<LPCSTR>(pMem));
	HeapFree(hProcessHeap, 0, pMem);

	Typer_RecordOpenFile(ofn.lpstrFile);
	
	return true;
}

LRESULT CALLBACK Typer_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		{
			state.hWndEdit = CreateWindowEx(
				WS_EX_CLIENTEDGE,
				L"Edit",
				L"",
				WS_CHILD | ES_MULTILINE,
				0,
				0,
				0,
				0,
				hWnd,
				nullptr,
				state.hInstance,
				nullptr
			);

			ShowWindow(state.hWndEdit, SW_SHOW);
			break;
		}

	case WM_SIZE:
		{
			RECT rect;
			GetClientRect(hWnd, &rect);

			MoveWindow(
				state.hWndEdit, 
				0, 
				0, 
				rect.right - rect.left, 
				rect.bottom - rect.top, 
				FALSE);
			
			break;
		}

	case WM_COMMAND:
		{
			const UINT id = LOWORD(wParam);

			switch (id)
			{
			case ID_TYPER_FILE_OPEN:
				Typer_OpenFile();
				break;

			case ID_TYPER_FILE_SAVE:
				Typer_Save();
				break;

			case ID_TYPER_FILE_SAVEAS:
				Typer_SaveAs();
				break;
				
			case ID_TYPER_FILE_EXIT:
				PostMessage(hWnd, WM_CLOSE, 0, 0);
				break;

			case ID_HELP_ABOUT:
				MessageBox(hWnd, L"Awesome", L"About Typer", 0);
				break;
			}
			
			break;
		}

	case CDN_HELP:
		MessageBox(hWnd, L"Please choose an existing file to open", L"Open a file", 0);
		break;

	case WM_CLOSE:
		{
			DestroyWindow(hWnd);
			break;
		}

	case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

ATOM Typer_RegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wnd = { 0 };
	
	wnd.cbSize = sizeof(wnd);
	wnd.style = CS_HREDRAW | CS_VREDRAW;
	wnd.lpszClassName = TYPER_WINDOWCLAZZ;
	wnd.hInstance = hInstance;
	wnd.lpfnWndProc = Typer_WndProc;
	wnd.lpszMenuName = MAKEINTRESOURCE(IDR_TYPER_MENU);

	return RegisterClassEx(&wnd);
}

DWORD Typer_ShowInitError(LPCWSTR stage)
{
	const DWORD err = GetLastError();

	LPTSTR lpBuffer;
	DWORD result = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr,
		err,
		LANG_SYSTEM_DEFAULT,
		LPTSTR(&lpBuffer),
		256,
		nullptr
	);
	
	wchar_t buffer[512] = {0};
	if (result == 0)
	{
		wsprintfW(buffer, L"Error during startup:\n%s [code: %lu]", stage, err);
	} else
	{
		wsprintfW(buffer, L"Error during startup: %s\n%s [code: %lu]", stage, lpBuffer, err);
	}
	
	MessageBox(nullptr, buffer, L"Startup error", MB_OK | MB_ICONSTOP | MB_DEFBUTTON1);

	return err;	
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow)
{
	ZeroMemory(&state, sizeof(state));
	
	const ATOM aWndClazz = Typer_RegisterClass(hInstance);
	if (!aWndClazz)
	{
		return Typer_ShowInitError(L"Failed to register window class");
	}

	HWND hWnd = CreateWindowEx(
		0,
		TYPER_WINDOWCLAZZ,
		L"Welcome to Typer",
		WS_OVERLAPPEDWINDOW,
		0,
		0,
		800,
		600,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);
	
	if (!hWnd)
	{
		return Typer_ShowInitError(L"Failed to create window");
	}

	state.hInstance = hInstance;
	state.hWnd = hWnd;
	
	ShowWindow(hWnd, nCmdShow);

	MSG msg = {0 };
	while (GetMessage(&msg, hWnd, 0, 0) && msg.message)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}	

	return 0;
}
