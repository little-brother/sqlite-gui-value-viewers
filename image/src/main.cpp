#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>
#include <gdiplus.h>
#include <stdio.h>

#define IDC_PLUGIN               100
#define PLUGIN_NAME              TEXT("image")
#define PLUGIN_VERSION           "1.0.0"

#define SQLITE_BLOB              4

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static TCHAR iniPath[MAX_PATH] = {0};

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
int getStoredValue(const TCHAR* name, int defValue);

HWND loadImage(HWND hParentWnd, const unsigned char* data, int dataLen, TCHAR* outInfo16, TCHAR* outExt16) {
	IStream* pStream = NULL;
	HRESULT hResult = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
	
	if(hResult != S_OK || !pStream) 
		return 0;

	hResult = pStream->Write(data, dataLen, NULL);

	if (hResult != S_OK) {
		pStream->Release();	
		return 0;
	}

	Gdiplus::Bitmap* pBmp = Gdiplus::Bitmap::FromStream(pStream);
	pStream->Release();

	RECT rc;
	GetClientRect(hParentWnd, &rc);
	int w = pBmp->GetWidth();
	int h = pBmp->GetHeight();

	GUID ff;
	pBmp->GetRawFormat(&ff);

	_tcscpy(outExt16, ff == Gdiplus::ImageFormatJPEG ? TEXT("jpeg") :
		ff == Gdiplus::ImageFormatBMP ? TEXT("bmp") :
		ff == Gdiplus::ImageFormatPNG ? TEXT("png") :
		ff == Gdiplus::ImageFormatGIF ? TEXT("gif") :
		ff == Gdiplus::ImageFormatTIFF ? TEXT("tiff") :
		TEXT("bin"));

	HWND hPluginWnd = 0;
	if (w > 0 && h > 0 && _tcscmp(outExt16, TEXT("bin"))) {
		_sntprintf(outInfo16, 255, TEXT("%ls %ix%i px"), outExt16, w, h);

		if (rc.right < w || rc.bottom < h) {
			float scale = MIN(rc.right/(float)w, rc.bottom/(float)h);
			w *= scale;
			h *= scale;

			Gdiplus::Bitmap* pBmp2 = new Gdiplus::Bitmap(w, h);
			Gdiplus::Graphics g(pBmp2);
			g.TranslateTransform(0, 0);
			g.DrawImage(pBmp, 0, 0, w, h);
			delete pBmp;
			pBmp = pBmp2;
		}

		HBITMAP hBmp = 0;
		pBmp->GetHBITMAP(0, &hBmp);

		hPluginWnd = CreateWindow(WC_STATIC, NULL, WS_CHILD | WS_VISIBLE | SS_REALSIZECONTROL | SS_CENTERIMAGE | SS_BITMAP | WS_CLIPSIBLINGS, 
			0, 0, 100, 100, hParentWnd, (HMENU)IDC_PLUGIN, GetModuleHandle(0), NULL);
		HBITMAP hOldBmp = (HBITMAP)SendMessage(hPluginWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
		if (hOldBmp)
			DeleteObject(hOldBmp);
		delete pBmp;
		InvalidateRect(hPluginWnd, NULL, TRUE);
	}
	
	return hPluginWnd;
}

extern "C" { 
	__declspec(dllexport) HWND view (HWND hParentWnd, const unsigned char* data, int dataLen, int dataType, TCHAR* outInfo16, TCHAR* outExt16) {
		return dataType == SQLITE_BLOB ? loadImage (hParentWnd, data, dataLen, outInfo16, outExt16) : 0;
	}
	
	__declspec(dllexport) int close (HWND hPluginWnd) {
		DestroyWindow(hPluginWnd);
		return 1;
	}
		
	__declspec(dllexport) int getPriority () {
		return getStoredValue(TEXT("priority"), 10);
	}
}

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH && iniPath[0] == 0) {
		TCHAR path[MAX_PATH + 1] = {0};
		GetModuleFileName((HMODULE)hModule, path, MAX_PATH);
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