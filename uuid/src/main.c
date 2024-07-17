#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <locale.h>
#include <commctrl.h>
#include <richedit.h>
#include <stdio.h>
#include <tchar.h>
#include <math.h>

#define IDC_PLUGIN               100
#define PLUGIN_NAME              TEXT("uuid")
#define PLUGIN_VERSION           "1.0.0"

#define UUID_LENGTH              38
#define SQLITE_BLOB              4

static TCHAR iniPath[MAX_PATH] = {0};

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
int getStoredValue(const TCHAR* name, int defValue);


HWND __stdcall view (HWND hParentWnd, const unsigned char* data, int dataLen, int dataType, TCHAR* outInfo16, TCHAR* outExt16) {
	if (dataType != SQLITE_BLOB || dataLen != 16)
		return 0;

	TCHAR uuid16[UUID_LENGTH];
	_sntprintf(uuid16, UUID_LENGTH, TEXT("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"), 
		data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
		data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]
	);
	
	if (getStoredValue(TEXT("uppercase"), 0) == 1) 
		CharUpper(uuid16);

	HWND hPluginWnd = CreateWindow(TEXT("RICHEDIT50W"), uuid16, 
		WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | WS_CLIPSIBLINGS, 
		0, 0, 100, 100, hParentWnd, (HMENU)IDC_PLUGIN, GetModuleHandle(0), NULL);

	HFONT hFont = (HFONT)SendMessage(hParentWnd, WM_GETFONT, 0, 0);
	SendMessage(hPluginWnd, WM_SETFONT, (LPARAM)hFont, 0);
	SendMessage(hPluginWnd, EM_SETTARGETDEVICE, 0, FALSE);

	_sntprintf(outInfo16, 5, TEXT("UUID"));
	_sntprintf(outExt16, 32, TEXT("uuid"));

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