#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <locale.h>
#include <stdio.h>
#include <tchar.h>
#include <commctrl.h>
#include "parson.h"

#define IDC_PLUGIN               100
#define PLUGIN_NAME              TEXT("json")
#define PLUGIN_VERSION           "1.0.0"

#define SQLITE_BLOB              4
#define SQLITE_TEXT              3
#define IDM_COPY_KEY             1000
#define IDM_COPY_VALUE           1001

#define MAX_KEY_LENGTH           4096
#define CP_UTF16LE               1200
#define CP_UTF16BE               1201

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
int getStoredValue(const TCHAR* name, int defValue);
LRESULT CALLBACK cbNewPlugin(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
HTREEITEM addNode(HWND hTreeWnd, HTREEITEM hParentItem, JSON_Value* val);
char* json_value_get_as_string(const JSON_Value* val);
TCHAR* utf8to16(const char* in);
char* utf16to8(const TCHAR* in);
int detectCodePage(const unsigned char *data);
void setClipboardText(const TCHAR* text);
BOOL isUtf8(const char * string);
HTREEITEM TreeView_AddItem (HWND hTreeWnd, TCHAR* caption, HTREEITEM parent, LPARAM lParam);
void TreeView_ExpandSubtree(HWND hTreeWnd, HTREEITEM hItem);
int TreeView_GetItemText(HWND hTreeWnd, HTREEITEM hItem, TCHAR* buf, int maxLen);
LPARAM TreeView_GetItemParam(HWND hTreeWnd, HTREEITEM hItem);
int TreeView_SetItemText(HWND hTreeWnd, HTREEITEM hItem, TCHAR* text);

static TCHAR iniPath[MAX_PATH] = {0};

HWND __stdcall view (HWND hParentWnd, const unsigned char* data, int dataLen, int dataType, TCHAR* outInfo16, TCHAR* outExt16) {
	int cp = CP_UTF8;
	unsigned char* data8 = 0;
	if (dataType == SQLITE_TEXT)
		data8 = utf16to8((TCHAR*)data);

	if (dataType == SQLITE_BLOB) {
		unsigned char* text = calloc(dataLen + 2, sizeof(char));
		memcpy(text, data, dataLen);

		cp = detectCodePage(text);	 	
		if (cp == CP_ACP) {
			DWORD len = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
			TCHAR* data16 = (TCHAR*)calloc (len, sizeof (TCHAR));
			MultiByteToWideChar(CP_ACP, 0, text, -1, data16, len);
	
			data8 = utf16to8(data16);
			free(data16);
		}
	
		if (cp == CP_UTF16LE) 
			data8 = utf16to8((TCHAR*)text);

		if (cp == CP_UTF16BE) {
			for (int i = 0; cp == CP_UTF16BE && i < dataLen / 2; i++) {
				int c = text[2 * i];
				text[2 * i] = text[2 * i + 1];
				text[2 * i + 1] = c;
			}
	
			data8 = utf16to8((TCHAR*)text);
		}

		if (cp == CP_UTF8) {
			data8 = calloc(dataLen + 2, sizeof(char));
			memcpy(data8, text, dataLen);
		}

		free(text);
	}

	if (data8 == 0) 
		return 0;

	json_set_escape_slashes(0);
	int offset = strncmp(data8, "\xEF\xBB\xBF", 3) == 0 ? 3 : 0;
	JSON_Value* json = json_parse_string(data8 + offset);
	free(data8);

	if (!json)
		return 0;

	JSON_Value_Type rootType = json_value_get_type(json);
	if (rootType != JSONArray && rootType != JSONObject) {
		json_value_free(json);
		return 0;
	}

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(icex);
	icex.dwICC = ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	HWND hTreeWnd = CreateWindow(WC_TREEVIEW, NULL, WS_VISIBLE | WS_CHILD | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT  | WS_TABSTOP | WS_CLIPSIBLINGS,
		0, 0, 100, 100, hParentWnd, (HMENU)IDC_PLUGIN, GetModuleHandle(0), NULL);
	SetProp(hTreeWnd, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hTreeWnd, GWLP_WNDPROC, (LONG_PTR)&cbNewPlugin));
	HFONT hFont = (HFONT)SendMessage(hParentWnd, WM_GETFONT, 0, 0);
	SendMessage(hTreeWnd, WM_SETFONT, (LPARAM)hFont, 0);

	HTREEITEM hRoot = TreeView_AddItem(hTreeWnd, TEXT("<<root>>"), TVI_ROOT, (LPARAM)json);
	addNode(hTreeWnd, hRoot, json);
	TreeView_Select(hTreeWnd, hRoot, TVGN_CARET);
	TreeView_ExpandSubtree(hTreeWnd, hRoot);


	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, IDM_COPY_KEY, TEXT("Copy key"));
	AppendMenu(hMenu, MF_STRING, IDM_COPY_VALUE, TEXT("Copy value"));
	SetProp(hTreeWnd, TEXT("MENU"), hMenu);

	SetProp(hTreeWnd, TEXT("JSON"), json);


	_sntprintf(outInfo16, 255, TEXT("json, %ls"),
		cp == CP_ACP ? TEXT("ANSI") :
		cp == CP_UTF16BE ? TEXT("UTF16be") :
		cp == CP_UTF16LE ? TEXT("UTF16le") : 
		TEXT("UTF8"));
	_sntprintf(outExt16, 32, TEXT("json"));

	return hTreeWnd;
}

int __stdcall close (HWND hPluginWnd) {
	json_value_free((JSON_Value*)GetProp(hPluginWnd, TEXT("JSON")));
	DestroyMenu(GetProp(hPluginWnd, TEXT("MENU")));

	RemoveProp(hPluginWnd, TEXT("JSON"));
	RemoveProp(hPluginWnd, TEXT("MENU"));

	DestroyWindow(hPluginWnd);
	return 1;
}

int __stdcall getPriority () {
	return getStoredValue(TEXT("priority"), 5);
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
	switch (msg) {
		case WM_COMMAND: {
			if (wParam == IDM_COPY_KEY || wParam == IDM_COPY_VALUE) {
				HTREEITEM hItem = TreeView_GetSelection(hWnd);
				if (hItem == NULL) 
					return 0;

				BOOL hasChild = TreeView_GetChild(hWnd, hItem) != NULL;
				TCHAR key16[MAX_KEY_LENGTH];
				TreeView_GetItemText(hWnd, hItem, key16, MAX_KEY_LENGTH);
				int pos = (int)(_tcschr(key16, TEXT(':')) - key16);

				if (wParam == IDM_COPY_KEY) {
					if (!hasChild) 
						key16[pos] = 0;
					setClipboardText(key16);
				}

				if (wParam == IDM_COPY_VALUE) {
					if (hasChild) {
						JSON_Value* val = (JSON_Value*)TreeView_GetItemParam(hWnd, hItem);
						char* json8 = json_serialize_to_string_pretty(val);
						TCHAR* json16 = utf8to16(json8);
						json_free_serialized_string(json8);
						setClipboardText(json16);
						free(json16);
					} else {
						setClipboardText(key16 + pos + 2 /* skip ': ' */);
					} 
				}
			}
		}
		break;

		case WM_RBUTTONDOWN: {
			POINT p = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			ClientToScreen(hWnd, &p);
			TrackPopupMenu(GetProp(hWnd, TEXT("MENU")), TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hWnd, NULL);
		}
		break;
	}

	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
}

HTREEITEM addNode(HWND hTreeWnd, HTREEITEM hParentItem, JSON_Value* val) {
	HTREEITEM hItem = 0;

	JSON_Value_Type type = json_value_get_type(val);
	if (type == JSONArray) {
		JSON_Array* arr = json_value_get_array(val);
		int cnt = json_array_get_count(arr);
		for (int itemNo = 0; itemNo < cnt; itemNo++) {
			JSON_Value* val2 = json_array_get_value(arr, cnt - itemNo - 1);
			TCHAR itemName[64];
			_sntprintf(itemName, 64, TEXT("[%i]"), cnt - itemNo - 1);
			hItem = TreeView_AddItem(hTreeWnd, itemName, hParentItem, (LPARAM)val2);
			addNode(hTreeWnd, hItem, val2);
		}

		return hItem;
	}

	if (type == JSONObject) {
		JSON_Object* obj = json_value_get_object(val);
		int cnt = json_object_get_count(obj);
		for (int itemNo = 0; itemNo < cnt; itemNo++) {
			JSON_Value* val2 = json_object_get_value_at(obj, cnt - itemNo - 1);
			TCHAR* itemName = utf8to16(json_object_get_name(obj, cnt - itemNo - 1));
			hItem = TreeView_AddItem(hTreeWnd, itemName, hParentItem, (LPARAM)val2);
			addNode(hTreeWnd, hItem, val2);
			free(itemName);
		}

		return hItem;
	}

	char* value8 = json_value_get_as_string(val);
	TCHAR* value16 = utf8to16(value8);
	free(value8);

	TCHAR key16[MAX_KEY_LENGTH + 1];
	TreeView_GetItemText(hTreeWnd, hParentItem, key16, MAX_KEY_LENGTH);

	int len = _tcslen(key16) + _tcslen(value16) + 3;
	TCHAR pair16[len + 1];
	_sntprintf(pair16, len + 1, TEXT("%ls: %ls"), key16, value16);
	TreeView_SetItemText(hTreeWnd, hParentItem, pair16);
	free(value16);

	return hParentItem;
}

char* json_value_get_as_string(const JSON_Value* val) {
	char* res = 0;
	int type = json_value_get_type(val);
	if (type == JSONObject) {
		res = calloc(10, sizeof(char));
		snprintf(res, 10, "[Object]");
	} else if (type == JSONArray) {
		res = calloc(10, sizeof(char));
		snprintf(res, 10, "[Array]");
	} else if (type == JSONString) {
		int len = json_value_get_string_len(val);
		res = calloc(len + 1, sizeof(char));
		strncpy(res, json_value_get_string(val), len);
	} else {
		char* str = json_serialize_to_string_pretty(val);
		int len = strlen(str);
		res = calloc(len + 1, sizeof(char));
		strncpy(res, str, len);
		json_free_serialized_string(str);
	}

	return res;
}

TCHAR* utf8to16(const char* in) {
	TCHAR *out;
	if (!in || strlen(in) == 0) {
		out = (TCHAR*)calloc (1, sizeof (TCHAR));
	} else  {
		DWORD size = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
		out = (TCHAR*)calloc (size, sizeof (TCHAR));
		MultiByteToWideChar(CP_UTF8, 0, in, -1, out, size);
	}
	return out;
}

char* utf16to8(const TCHAR* in) {
	char* out;
	if (!in || _tcslen(in) == 0) {
		out = (char*)calloc (1, sizeof(char));
	} else {
		int len = WideCharToMultiByte(CP_UTF8, 0, in, -1, NULL, 0, 0, 0);
		out = (char*)calloc (len, sizeof(char));
		WideCharToMultiByte(CP_UTF8, 0, in, -1, out, len, 0, 0);
	}
	return out;
}

int detectCodePage(const unsigned char *data) {
	return strncmp(data, "\xEF\xBB\xBF", 3) == 0 ? CP_UTF8 : // BOM
		strncmp(data, "\xFE\xFF", 2) == 0 ? CP_UTF16BE : // BOM
		strncmp(data, "\xFF\xFE", 2) == 0 ? CP_UTF16LE : // BOM
		strncmp(data, "\x00\x5B", 2) == 0 || strncmp(data, "\x00\x7B", 2) == 0 ? CP_UTF16BE : // [ {		
		strncmp(data, "\x5B\x00", 2) == 0 || strncmp(data, "\x7B\x00", 2) == 0 ? CP_UTF16LE : // [ {
		strncmp(data, "\x2F\x00\x2F\x00", 4) == 0 ? CP_UTF16BE : // //
		strncmp(data, "\x00\x2F\x00\x2F", 4) == 0 ? CP_UTF16LE : // //
		isUtf8(data) ? CP_UTF8 :		
		CP_ACP;	
}

void setClipboardText(const TCHAR* text) {
	int len = (_tcslen(text) + 1) * sizeof(TCHAR);
	HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(hMem), text, len);
	GlobalUnlock(hMem);
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, hMem);
	CloseClipboard();
}

// https://stackoverflow.com/a/1031773/6121703
BOOL isUtf8(const char * string) {
	if (!string)
		return FALSE;

	const unsigned char * bytes = (const unsigned char *)string;
	while (*bytes) {
		if((bytes[0] == 0x09 || bytes[0] == 0x0A || bytes[0] == 0x0D || (0x20 <= bytes[0] && bytes[0] <= 0x7E))) {
			bytes += 1;
			continue;
		}

		if (((0xC2 <= bytes[0] && bytes[0] <= 0xDF) && (0x80 <= bytes[1] && bytes[1] <= 0xBF))) {
			bytes += 2;
			continue;
		}

		if ((bytes[0] == 0xE0 && (0xA0 <= bytes[1] && bytes[1] <= 0xBF) && (0x80 <= bytes[2] && bytes[2] <= 0xBF)) ||
			(((0xE1 <= bytes[0] && bytes[0] <= 0xEC) || bytes[0] == 0xEE || bytes[0] == 0xEF) && (0x80 <= bytes[1] && bytes[1] <= 0xBF) && (0x80 <= bytes[2] && bytes[2] <= 0xBF)) ||
			(bytes[0] == 0xED && (0x80 <= bytes[1] && bytes[1] <= 0x9F) && (0x80 <= bytes[2] && bytes[2] <= 0xBF))
		) {
			bytes += 3;
			continue;
		}

		if ((bytes[0] == 0xF0 && (0x90 <= bytes[1] && bytes[1] <= 0xBF) && (0x80 <= bytes[2] && bytes[2] <= 0xBF) && (0x80 <= bytes[3] && bytes[3] <= 0xBF)) ||
			((0xF1 <= bytes[0] && bytes[0] <= 0xF3) && (0x80 <= bytes[1] && bytes[1] <= 0xBF) && (0x80 <= bytes[2] && bytes[2] <= 0xBF) && (0x80 <= bytes[3] && bytes[3] <= 0xBF)) ||
			(bytes[0] == 0xF4 && (0x80 <= bytes[1] && bytes[1] <= 0x8F) && (0x80 <= bytes[2] && bytes[2] <= 0xBF) && (0x80 <= bytes[3] && bytes[3] <= 0xBF))
		) {
			bytes += 4;
			continue;
		}

		return FALSE;
	}

	return TRUE;
}

HTREEITEM TreeView_AddItem (HWND hTreeWnd, TCHAR* caption, HTREEITEM parent, LPARAM lParam) {
	TVITEM tvi = {0};
	TVINSERTSTRUCT tvins = {0};
	tvi.mask = TVIF_TEXT | TVIF_PARAM;
	tvi.pszText = caption;
	tvi.cchTextMax = _tcslen(caption) + 1;
	tvi.lParam = lParam;

	tvins.item = tvi;
	tvins.hInsertAfter = TVI_FIRST;
	tvins.hParent = parent;
	return (HTREEITEM)SendMessage(hTreeWnd, TVM_INSERTITEM, 0, (LPARAM)(LPTVINSERTSTRUCT)&tvins);
};

void TreeView_ExpandSubtree(HWND hTreeWnd, HTREEITEM hItem) {
	TreeView_Expand(hTreeWnd, hItem, TVE_EXPAND);

	HTREEITEM tv = TreeView_GetChild(hTreeWnd, hItem);
	if (tv == NULL)
		return;

	do {		
		TreeView_ExpandSubtree(hTreeWnd, tv);
		tv = TreeView_GetNextSibling(hTreeWnd, tv);
	} while (tv != NULL);
}

int TreeView_GetItemText(HWND hTreeWnd, HTREEITEM hItem, TCHAR* buf, int maxLen) {
	TV_ITEM tv = {0};
	tv.mask = TVIF_TEXT;
	tv.hItem = hItem;
	tv.cchTextMax = maxLen;
	tv.pszText = buf;
	return TreeView_GetItem(hTreeWnd, &tv);
}

LPARAM TreeView_GetItemParam(HWND hTreeWnd, HTREEITEM hItem) {
	TV_ITEM tv = {0};
	tv.mask = TVIF_PARAM;
	tv.hItem = hItem;

	return TreeView_GetItem(hTreeWnd, &tv) ? tv.lParam : 0;
}

int TreeView_SetItemText(HWND hTreeWnd, HTREEITEM hItem, TCHAR* text) {
	TV_ITEM tv = {0};
	tv.mask = TVIF_TEXT;
	tv.hItem = hItem;
	tv.cchTextMax = _tcslen(text) + 1;
	tv.pszText = text;
	return TreeView_SetItem(hTreeWnd, &tv);
}