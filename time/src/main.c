#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <richedit.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>

#define IDC_PLUGIN               100
#define PLUGIN_NAME              TEXT("time")
#define PLUGIN_VERSION           "1.0.0"

#define SQLITE_TEXT              3

static TCHAR iniPath[MAX_PATH] = {0};

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
int getStoredValue(const TCHAR* name, int defValue);
TCHAR* getStoredString(TCHAR* name, TCHAR* defValue);

HWND __stdcall view (HWND hParentWnd, const unsigned char* data, int dataLen, int dataType, TCHAR* outInfo16, TCHAR* outExt16) {
	if (dataType != SQLITE_TEXT || (dataLen != 10 && dataLen != 13))
		return 0;

	for (int i = 0; i < dataLen; i++) {
		if (!_istdigit(data[i]))
			return 0;
	}

	time_t rawtime = _ttoi64((TCHAR*)data) / (dataLen == 13 ? 1000 : 1);
	if (rawtime > 2120000000) // 07.03.2037
		return 0;

	struct tm* ptm = gmtime (&rawtime);

	TCHAR time16[256];
	TCHAR* format16 = getStoredString(TEXT("format"), TEXT("%d-%m-%Y %H:%M:%S, %A"));
	_tcsftime(time16, 256, format16, ptm);
	free(format16);

	TCHAR buf16[256];
	_sntprintf(buf16, 256, TEXT("Value: %ls\nTime: %ls"), (TCHAR*)data, time16);

	HWND hPluginWnd = CreateWindow(TEXT("RICHEDIT50W"), buf16, 
		WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | WS_CLIPSIBLINGS, 
		0, 0, 100, 100, hParentWnd, (HMENU)IDC_PLUGIN, GetModuleHandle(0), NULL);

	HFONT hFont = (HFONT)SendMessage(hParentWnd, WM_GETFONT, 0, 0);
	SendMessage(hPluginWnd, WM_SETFONT, (LPARAM)hFont, 0);
	SendMessage(hPluginWnd, EM_SETTARGETDEVICE, 0, FALSE);

	_sntprintf(outExt16, 32, TEXT("txt"));
	_sntprintf(outInfo16, 255, TEXT("Timestampt"));

	return hPluginWnd;
}

int __stdcall close (HWND hPluginWnd) {
	DestroyWindow(hPluginWnd);
	return 1;
}

int __stdcall getPriority () {
	return getStoredValue(TEXT("priority"), 1);
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

TCHAR* getStoredString(TCHAR* name, TCHAR* defValue) { 
	TCHAR* buf = calloc(256, sizeof(TCHAR));
	if (0 == GetPrivateProfileString(PLUGIN_NAME, name, NULL, buf, 128, iniPath) && defValue)
		_tcsncpy(buf, defValue, 255);
	return buf;	
}