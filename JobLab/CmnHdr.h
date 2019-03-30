#pragma once
#include <windows.h>

#define chDIMOF(Array) (sizeof(Array) / sizeof(Array[0]))

inline void chMB(PCSTR s) {
	char szTMP[128];
	GetModuleFileNameA(NULL, szTMP, chDIMOF(szTMP));
	MessageBoxA(GetActiveWindow(), s, szTMP, MB_OK);
}

inline void chFAIL(PSTR szMsg) {
	chMB(szMsg);
	DebugBreak();
}

inline void chASSERTFAIL(LPCSTR file, int line, PCSTR expr) {
	char sz[128];
	wsprintfA(sz, "File %s, line %d : %s", file, line, expr);
	chFAIL(sz);
}

#ifdef _DEBUG
#define chASSERT(x) if (!(x)) chASSERTFAIL(__FILE__, __LINE__, #x)
#else
#define chASSERT(x)
#endif

#ifdef _DEBUG
#define chVERIFY(x) chASSERT(x)
#else
#define chVERIFY(x) (x)
#endif

#define chHANDLE_DLGMSG(hwnd, message, fn) \
	case (message): return (SetDlgMsgResult(hwnd, uMsg, HANDLE_##message((hwnd), (wParam), (lParam), (fn))))

typedef unsigned(__stdcall* PTHREAD_START) (void*);

#define chBEGINTHREADEX(psa, cbStack, pfnStartAddr, pvParam, fdwCreate, pdwThreadId) \
	((HANDLE)_beginthreadex(\
		(void *) (psa),\
		(unsigned) (cbStack),\
		(PTHREAD_START) (pfnStartAddr),\
		(void *) (pvParam),\
		(unsigned) (fdwCreate),\
		(unsigned *) (pdwThreadId)))

inline void chSETDLGICONS(HWND hwnd, int idi) {
	SendMessage(hwnd, WM_SETICON, TRUE, (LPARAM)LoadIcon((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), MAKEINTRESOURCE(idi)));
	SendMessage(hwnd, WM_SETICON, FALSE, (LPARAM)LoadIcon((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), MAKEINTRESOURCE(idi)));
}