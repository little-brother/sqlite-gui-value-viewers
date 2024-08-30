#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <stdio.h>
#include <uxtheme.h>
#include <richedit.h>

#define IDC_PLUGIN               100
#define PLUGIN_NAME              TEXT("text")
#define PLUGIN_VERSION           "1.0.1"

#define SQLITE_INTEGER           1
#define SQLITE_FLOAT             2
#define SQLITE_TEXT              3
#define SQLITE_BLOB              4
#define SQLITE_NULL              5

static TCHAR iniPath[MAX_PATH] = {0};

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
int getStoredValue(const TCHAR* name, int defValue);
void setStoredValue(TCHAR* name, int value);
LRESULT CALLBACK cbNewPlugin(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
POINTFLOAT getWndScale(HWND hWnd);

HWND __stdcall view (HWND hParentWnd, const unsigned char* data, int dataLen, int dataType, TCHAR* outInfo16, TCHAR* outExt16) {
	if (dataType == SQLITE_BLOB || dataType == SQLITE_NULL)
		return 0;

	HWND hPluginWnd = CreateWindow(TEXT("RICHEDIT50W"), (TCHAR*)data, 
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL | ES_READONLY | WS_CLIPSIBLINGS, 
		0, 0, 100, 100, hParentWnd, (HMENU)IDC_PLUGIN, GetModuleHandle(0), NULL);
	SetProp(hPluginWnd, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hPluginWnd, GWLP_WNDPROC, (LONG_PTR)&cbNewPlugin));
	HFONT hFont = (HFONT)SendMessage(hParentWnd, WM_GETFONT, 0, 0);
	SendMessage(hPluginWnd, WM_SETFONT, (LPARAM)hFont, 0);

	RECT rc;
	GetClientRect(hPluginWnd, &rc);
	POINTFLOAT s = getWndScale(hParentWnd);
	InflateRect(&rc, -5 * s.x, -5 * s.y);
	SendMessage(hPluginWnd, EM_SETRECT, 0, (LPARAM)&rc);

	SendMessage(hPluginWnd, EM_SETTARGETDEVICE, 0, !getStoredValue(TEXT("word-wrap"), 0));

	_sntprintf(outInfo16, 32, TEXT("Text"));
	_sntprintf(outExt16, 32, TEXT("txt"));

	return hPluginWnd;
}

int __stdcall close (HWND hPluginWnd) {
	DestroyWindow(hPluginWnd);
	return 1;
}

int __stdcall getPriority () {
	return getStoredValue(TEXT("priority"), 0);
}

LRESULT CALLBACK cbNewPlugin(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_KEYDOWN && wParam == 0x57 && HIWORD(GetKeyState(VK_CONTROL))) { // Ctrl + W
		int isWordWrap = getStoredValue(TEXT("word-wrap"), 0);
		SendMessage(hWnd, EM_SETTARGETDEVICE, 0, isWordWrap);
		setStoredValue(TEXT("word-wrap"), !isWordWrap);
	}

	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
}


BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH && iniPath[0] == 0) {
		TCHAR path[MAX_PATH + 1] = {0};
		GetModuleFileName((HMODULE)hModule, path, MAX_PATH);
		TCHAR* dot = _tcsrchr(path, TEXT('.'));
		_tcsncpy(dot, TEXT(".ini"), 5);
		_tcscpy(iniPath, path);	
	}
	return TRUE;
}

int getStoredValue(const TCHAR* name, int defValue) {
	TCHAR buf[128];
	return GetPrivateProfileString(PLUGIN_NAME, name, NULL, buf, 128, iniPath) ? _ttoi(buf) : defValue;
}

POINTFLOAT getWndScale(HWND hWnd) {
	HDC hDC = GetDC(hWnd);
	POINTFLOAT z = {GetDeviceCaps(hDC, LOGPIXELSX) / 96.f, GetDeviceCaps(hDC, LOGPIXELSY) / 96.f};
	ReleaseDC(hWnd, hDC);

	return z;
}

void setStoredValue(TCHAR* name, int value) {
	fclose(_tfopen(iniPath, TEXT("a")));
	
	TCHAR buf[128];
	_sntprintf(buf, 128, TEXT("%i"), value);
	WritePrivateProfileString(PLUGIN_NAME, name, buf, iniPath);
}