#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <locale.h>
#include <stdio.h>
#include <tchar.h>
#include <math.h>
#include <richedit.h>

#define IDC_PLUGIN               100
#define PLUGIN_NAME              TEXT("binary")
#define PLUGIN_VERSION           "1.0.0"

#define SQLITE_BLOB              4

static TCHAR iniPath[MAX_PATH] = {0};

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
int getStoredValue(const TCHAR* name, int defValue);

#define xhr(i) ((i) < dataLen ? data[i] : 0xFF)
#define chr(i) ((i) >= dataLen ? ' ' : (data[(i)] >= 0x20 && data[(i)] <= 0x7E) || data[(i)] >= 0x80 ? (char)data[(i)] : '.')

HWND __stdcall view (HWND hParentWnd, const unsigned char* data, int dataLen, int dataType, TCHAR* outInfo16, TCHAR* outExt16) {
	if (dataType != SQLITE_BLOB)
		return 0;

	HWND hPluginWnd = CreateWindow(TEXT("RICHEDIT50W"), TEXT("Loading..."), 
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL | ES_READONLY | WS_CLIPSIBLINGS, 
		0, 0, 100, 100, hParentWnd, (HMENU)IDC_PLUGIN, GetModuleHandle(0), NULL);

	HFONT hFont = (HFONT)SendMessage(hParentWnd, WM_GETFONT, 0, 0);
	SendMessage(hPluginWnd, WM_SETFONT, (LPARAM)hFont, 0);

	int maxDataLen = getStoredValue(TEXT("max-length"), 65536);
	if (maxDataLen != 0 && dataLen >= maxDataLen) {
		dataLen = maxDataLen;
		int mag = floor(log10(dataLen)/log10(1024));
		double tosize = (double) dataLen / (1L << (mag * 10));
		const TCHAR* sizes[] = {TEXT("b"), TEXT("KB"), TEXT("MB"), TEXT("GB"), TEXT("TB")};
		_sntprintf(outInfo16, 255, TEXT("First %.2lf%s"), tosize, mag < 5 ? sizes[mag] : NULL);
	}

	// each byte requires: 3 byte for hex e.g. `FA `, 1 byte for char, 4 bytes per each 8 char 
	// for ` | ` + \n and additional 36 byte for last line
	char* hex = (char*)calloc (dataLen * 4 + (dataLen / 8) * 4 + 36 + 1, sizeof (char));

	int lastLineNo = dataLen / 8;
	for (int lineNo = 0; lineNo <= lastLineNo; lineNo++) {
		char buf[36] = {0};
		sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X | %c%c%c%c%c%c%c%c\n",
			xhr(lineNo * 8), xhr(lineNo * 8 + 1), xhr(lineNo * 8 + 2), xhr(lineNo * 8 + 3),
			xhr(lineNo * 8 + 4), xhr(lineNo * 8 + 5), xhr(lineNo * 8 + 6), xhr(lineNo * 8 + 7),
			chr(lineNo * 8), chr(lineNo * 8 + 1), chr(lineNo * 8 + 2), chr(lineNo * 8 + 3),
			chr(lineNo * 8 + 4), chr(lineNo * 8 + 5), chr(lineNo * 8 + 6), chr(lineNo * 8 + 7)
		);

		if (lineNo == dataLen / 8) {
			for (int i = 8 + lineNo * 8 - dataLen; i < 8; i++) {
				buf[3 * i] = ' ';
				buf[3 * i + 1] = ' ';
			}
		}

		memcpy(hex + lineNo * 35, buf, 36);
	}

	for (int charNo = 0; charNo < 8 - dataLen % 8; charNo++) {
		hex[lastLineNo * 33 + 33 - 3 * charNo + 2] = ' ';
		hex[lastLineNo * 33 + 33 - 3 * charNo + 3] = ' ';
	}

	DWORD hexSize = MultiByteToWideChar(CP_ACP, 0, hex, -1, NULL, 0);
	TCHAR* hex16 = (TCHAR*) calloc (hexSize + 1, sizeof (TCHAR));
	MultiByteToWideChar(CP_ACP, 0, hex, -1, hex16, hexSize);
	free(hex);

	SendMessage(hPluginWnd, EM_EXLIMITTEXT, 0, hexSize + 1);
	SetWindowText(hPluginWnd, hex16);
	free(hex16);

	_sntprintf(outExt16, 32, TEXT("blob"));

	return hPluginWnd;
}

int __stdcall close (HWND hPluginWnd) {
	DestroyWindow(hPluginWnd);
	return 1;
}

int __stdcall getPriority () {
	return getStoredValue(TEXT("priority"), 0);
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