#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <tchar.h>

#define IDC_PLUGIN               100
#define PLUGIN_NAME              TEXT("pdf")
#define PLUGIN_VERSION           "1.0.0"

#define SQLITE_BLOB              4

static TCHAR iniPath[MAX_PATH] = {0};

LRESULT CALLBACK cbNewPlugin(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall CALLBACK SpyProc(int nCode,WPARAM wParam, LPARAM lParam);
BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
int getStoredValue(const TCHAR* name, int defValue);
TCHAR* getStoredString(TCHAR* name, TCHAR* defValue);

HWND __stdcall view (HWND hParentWnd, const unsigned char* data, int dataLen, int dataType, TCHAR* outInfo16, TCHAR* outExt16) {
	if (dataType != SQLITE_BLOB)
		return 0;

	BOOL isPdf = dataLen > 10 && data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F' && 
		data[4] == '-' && data[5] == '1' && data[6] == '.' && (data[7] == '3' || data[7] == '4');

	if (!isPdf)
		return 0;

	TCHAR* sumatraPath16 = getStoredString(TEXT("sumatrapdf-path"), TEXT("./plugins/vvp/SumatraPDF.exe"));
	if (_taccess(sumatraPath16, 0) != 0) {
		MessageBox(NULL, TEXT("The SumatraPDF wasn't found. The default path is \"./plugins/vvp/SumatraPDF.exe\".\nYou can provide own path for \"sumatrapdf-path\"-key in pdf.ini."), NULL, 0);
		free(sumatraPath16);
		return 0;
	}

	HWND hPluginWnd = CreateWindow(TEXT("RICHEDIT50W"), NULL, 
		WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | WS_CLIPSIBLINGS, 
		0, 0, 100, 100, hParentWnd, (HMENU)IDC_PLUGIN, GetModuleHandle(0), NULL);
	SetProp(hPluginWnd, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hPluginWnd, GWLP_WNDPROC, (LONG_PTR)&cbNewPlugin));

	TCHAR TMP_PATH[MAX_PATH] = {0};
	GetTempPath(MAX_PATH, TMP_PATH);

	TCHAR filepath16[MAX_PATH];
	_sntprintf (filepath16, MAX_PATH, TEXT("%ls\\sqlite-gui\\blob-%i.pdf"), TMP_PATH, GetTickCount());
	FILE* f = _tfopen(filepath16, TEXT("wb"));
	fwrite(data, dataLen, 1, f);
	fclose(f);

	PROCESS_INFORMATION pi = {0};
	STARTUPINFOW si = {0};

	HWND hPdfWnd = NULL;
	TCHAR cmd16[4096];
	_sntprintf (cmd16, 4096, TEXT("%ls -plugin %d \"%ls\""), sumatraPath16, hPluginWnd, filepath16);
	free(sumatraPath16);
	si.cb = sizeof(si);
	if (CreateProcess(NULL, cmd16, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {	
		for(int i = 0; hPdfWnd == 0 && i < 5000; i++){
			Sleep(10);
			hPdfWnd = FindWindowEx(hPluginWnd, NULL, NULL, NULL);
		}

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	} 

	if (hPdfWnd) {
		SetWindowLongPtr(hPluginWnd, GWLP_USERDATA, (LONG_PTR)SetWindowsHookEx(WH_GETMESSAGE,
			(HOOKPROC)SpyProc, GetModuleHandle(0), pi.dwThreadId));
		_sntprintf(outExt16, 32, TEXT("pdf"));
	} else {
		DestroyWindow(hPluginWnd);
		hPluginWnd = 0;
	};

	return hPluginWnd;
}

int __stdcall close (HWND hPluginWnd) {
	UnhookWindowsHookEx((HHOOK)GetWindowLongPtr(hPluginWnd, GWLP_USERDATA));

	DestroyWindow(hPluginWnd);
	return 1;
}

int __stdcall getPriority () {
	return getStoredValue(TEXT("priority"), 10);
}

LRESULT __stdcall CALLBACK SpyProc(int nCode,WPARAM wParam, LPARAM lParam) {
	if(nCode == HC_ACTION) {
		PMSG pMsg = (PMSG)lParam;
		if(pMsg->message == WM_KEYDOWN){
			TCHAR cn[100];
			
			GetClassName(pMsg->hwnd, cn, 100);
			if (_tcscmp(cn,TEXT("SUMATRA_PDF_FRAME")) == 0 || _tcscmp(cn, TEXT("SysTreeView32")) == 0)
				PostMessage(GetParent(GetParent(pMsg->hwnd)), WM_KEYDOWN, pMsg->wParam, pMsg->lParam);
			if(pMsg->wParam == VK_TAB)
				pMsg->message = WM_NULL;
		}
	}
	return  CallNextHookEx( 0, nCode, wParam, lParam );
}

LRESULT CALLBACK cbNewPlugin(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_SIZE) {
		WORD w = LOWORD(lParam);
		WORD h = HIWORD(lParam);

		HWND hPdfWnd = GetWindow(hWnd, GW_CHILD);
		SetWindowPos(hPdfWnd, 0, 0, 0, w, h, SWP_NOZORDER);

		return 0;
	}

	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
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