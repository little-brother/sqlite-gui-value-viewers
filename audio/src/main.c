#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <stdio.h>
#include <uxtheme.h>
#include <vfw.h>
#include "mp3detect.h"

#define IDC_PLUGIN               100
#define PLUGIN_NAME              TEXT("audio")
#define PLUGIN_VERSION           "1.0.0"

#define SQLITE_BLOB              4

static TCHAR iniPath[MAX_PATH] = {0};

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
int getStoredValue(const TCHAR* name, int defValue);
LRESULT CALLBACK cbNewPlugin(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND __stdcall view (HWND hParentWnd, const unsigned char* data, int dataLen, int dataType, TCHAR* outInfo16, TCHAR* outExt16) {
	if (dataType != SQLITE_BLOB)
		return 0;

	TCHAR TMP_PATH[MAX_PATH] = {0};
	GetTempPath(MAX_PATH, TMP_PATH);

	BOOL isWav = dataLen > 44 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' && 
		data[8] == 'W' && data[9] == 'A' && data[10] == 'V' && data[11] == 'E';

	TCHAR ext16[32];
	_sntprintf(ext16, 32, is_mp3(data, dataLen) ? TEXT("mp3") : isWav ? TEXT("wav") : TEXT(""));
	TCHAR path16[MAX_PATH];
	_sntprintf(path16, MAX_PATH, TEXT("%ls\\sqlite-gui\\audio-%i.%ls"), TMP_PATH, GetTickCount(), ext16);

	// There is no simple way to play an in-memory file, so dump data to a disk
	// https://www.betaarchive.com/wiki/index.php?title=Microsoft_KB_Archive/155360
	FILE* f = _tfopen(path16, TEXT("wb"));
	fwrite(data, dataLen, 1, f);
	fclose(f);

	HWND hPluginWnd = CreateWindow(WC_STATIC, NULL, WS_CHILD | WS_CLIPSIBLINGS, 
		0, 0, 100, 100, hParentWnd, (HMENU)IDC_PLUGIN, GetModuleHandle(0), NULL);

	HWND hPlayerWnd = MCIWndCreate(hPluginWnd, GetModuleHandle(0), WS_CHILD | WS_VISIBLE | MCIWNDF_NOMENU | MCIWNDF_SHOWNAME | MCIWNDF_NOERRORDLG, path16);
	if (MCIWndCanPlay(hPlayerWnd)) {
		HFONT hFont = (HFONT)SendMessage(hParentWnd, WM_GETFONT, 0, 0);
		SendMessage(hPluginWnd, WM_SETFONT, (LPARAM)hFont, 0);

		LONG_PTR style = GetWindowLongPtr(hPlayerWnd, GWL_STYLE);
		style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
		SetWindowLongPtr(hPlayerWnd, GWL_STYLE, style);

		_sntprintf(outExt16, 32, ext16);
		_sntprintf(outInfo16, 32, TEXT("%ls, %is"), outExt16, MCIWndGetLength(hPlayerWnd) / 1000);

		SetProp(hPluginWnd, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hPluginWnd, GWLP_WNDPROC, (LONG_PTR)&cbNewPlugin));
		ShowWindow(hPluginWnd, SW_SHOW);
	} else {
		MCIWndDestroy(hPlayerWnd);
		DestroyWindow(hPluginWnd);
		hPluginWnd = 0;
	}

	return hPluginWnd;
}

int __stdcall close (HWND hPluginWnd) {
	HWND hPlayerWnd = GetWindow(hPluginWnd, GW_CHILD);
	MCIWndClose(hPlayerWnd);
	MCIWndDestroy(hPlayerWnd);
	DestroyWindow(hPluginWnd);
	return 1;
}

int __stdcall getPriority () {
	return getStoredValue(TEXT("priority"), 10);
}

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH && iniPath[0] == 0) {
		TCHAR path[MAX_PATH + 1] = {0};
		GetModuleFileName(hModule, path, MAX_PATH);
		TCHAR* dot = _tcsrchr(path, TEXT('.'));
		_tcsncpy(dot, TEXT(".ini"), 5);
		if (_taccess(path, 0) == 0)
			_tcscpy(iniPath, path);	
	}
	return TRUE;
}

int getStoredValue(const TCHAR* name, int defValue) {
	TCHAR buf[128];
	return GetPrivateProfileString(PLUGIN_NAME, name, NULL, buf, 128, iniPath) ? _ttoi(buf) : defValue;
}

LRESULT CALLBACK cbNewPlugin(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_SIZE) {
		WORD w = LOWORD(lParam);
		WORD h = HIWORD(lParam);

		HWND hPlayerWnd = GetWindow(hWnd, GW_CHILD);
		RECT rc;
		GetClientRect(hPlayerWnd, &rc);

		SetWindowPos(hPlayerWnd, 0, 0, h - rc.bottom - 20, w, rc.bottom, SWP_NOZORDER);

		return 0;
	}

	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
}