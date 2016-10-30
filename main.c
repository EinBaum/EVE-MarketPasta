#include <windows.h>
#include <stdlib.h>
#include <inttypes.h>

// Other stuff
#define MSG_CUSTOM_UPDATE	(WM_APP + 1)
#define NUMBER_BUFFER		(100)
#define COPY_BUFFER		(1000)

#define IDC_ENABLED	101
#define IDC_MINRAND	102
#define IDC_MAXRAND	103

HFONT defaultFont = NULL;

HWND hWnd = NULL;
HWND hWndEnabled, hWndStatic1, hWndStatic2, hWndMinRand, hWndMaxRand,
	hWndOld, hWndNew;
HWND hWndNextViewer = NULL;

BOOL progEnabled = TRUE;
BOOL ignoreChange = FALSE;

// Popup message box
void Show(WCHAR *str) {
	MessageBoxW(NULL, str, L"MarketPasta", 0);
}

// Checks whether a string contains market order that was
// copied from the market window
BOOL IsMarketData(WCHAR *str) {
	WCHAR *c;
	int numTabs = 0;
	int numLines = 0;
	for (c = str; *c != '\0'; c++) {
		if (*c == '\t') numTabs++;
		if (*c == '\n') numLines++;
	}
	return (numLines == 0) && (numTabs == 4);
}

// Extracts the price value from a market order string.
// Notice: Returns ISK-Cent, not ISK.
DWORD64 PriceFromMarketData(WCHAR *str) {
	WCHAR *sep = L"\t";
	WCHAR copy[COPY_BUFFER];
	WCHAR *tok;
	DWORD64 result = 0;

	// Copy string to use strtok
	wcscpy_s(copy, COPY_BUFFER, str);

	// Columns: Jumps, Quantity, Price, Location, Price
	// Get price (3rd column)
	tok = wcstok(copy, sep); if (!tok) return 0;
	tok = wcstok(NULL, sep); if (!tok) return 0;
	tok = wcstok(NULL, sep); if (!tok) return 0;

	for ( ; *tok != '\0'; tok++) {
		if (isdigit(*tok)) {
			result *= 10;
			result += (*tok - '0');
		}
	}

	return result;
}

// Calculates the undercut price using the old ISK-Cent price.
DWORD64 CalcNewPrice(DWORD64 oldPrice) {
	WCHAR buff[NUMBER_BUFFER];
	DWORD64 minRand, maxRand;

	// Get input fields for minRand, maxRand
	// Invalid inputs are read as 0
	GetWindowTextW(hWndMinRand, buff, NUMBER_BUFFER);
	minRand = _wcstoui64(buff, NULL, 10);

	GetWindowTextW(hWndMaxRand, buff, NUMBER_BUFFER);
	maxRand = _wcstoui64(buff, NULL, 10);

	// Always undercut by at least 1
	if (minRand == 0) {
		minRand = 1;
	}

	if (maxRand <= minRand) {
		return oldPrice - minRand;
	} else {
		return oldPrice - minRand - (rand() % (maxRand - minRand));
	}
}

// Show old and new price on the interface
void ShowPrices(DWORD64 oldPrice, DWORD64 newPrice) {
	WCHAR buff[NUMBER_BUFFER];

	wsprintfW(buff, L"Old Price:\t%" PRIu64 ",%02" PRIu64 " ISK",
		oldPrice/100, oldPrice%100);
	SetWindowTextW(hWndOld, buff);

	wsprintfW(buff, L"New Price:\t%" PRIu64 ",%02" PRIu64 " ISK",
		newPrice/100, newPrice%100);
	SetWindowTextW(hWndNew, buff);
}

// Changes clipboard contents:
// A market order string is replaced with a new price that can be used to
// undercut the copied market order.
void ModifyClipboard(HWND hWnd, WCHAR *str) {
	HGLOBAL hClipboardData;
	WCHAR newStr[NUMBER_BUFFER];
	WCHAR *pchData;
	DWORD64 oldPrice, newPrice;

	ignoreChange = TRUE;

	oldPrice = PriceFromMarketData(str);
	newPrice = CalcNewPrice(oldPrice);

	ShowPrices(oldPrice, newPrice);
	wsprintfW(newStr, L"%" PRIu64 ",%" PRIu64, newPrice/100, newPrice%100);


	if (OpenClipboard(hWnd)) {
		hClipboardData = GlobalAlloc(GMEM_MOVEABLE, sizeof(newStr));
		pchData = (WCHAR*) GlobalLock(hClipboardData);
		memcpy(pchData, newStr, sizeof(newStr));
		GlobalUnlock(hClipboardData);
		SetClipboardData(CF_UNICODETEXT, hClipboardData);
		CloseClipboard();
	}

	ignoreChange = FALSE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HGLOBAL hGlobal;
	WCHAR *pGlobal;

	switch (message)
	{
		case WM_CREATE:
			hWndNextViewer = SetClipboardViewer(hWnd);

			hWndEnabled = CreateWindowW(L"button", L"Enabled",
				WS_CHILD | WS_VISIBLE | BS_CHECKBOX | WS_TABSTOP,
				5, 5, 150, 20, hWnd, (HMENU)IDC_ENABLED,
				GetModuleHandleW(NULL), NULL);

			hWndStatic1 = CreateWindowW(L"static", L"Minimum Undercut (ISK-Cent)",
				WS_CHILD | WS_VISIBLE,
				5, 35, 150, 18, hWnd, 0,
				GetModuleHandleW(NULL), NULL);

			hWndMinRand = CreateWindowW(L"edit", 0,
				WS_CHILD | WS_BORDER | WS_VISIBLE | WS_TABSTOP,
				5, 50, 135, 18, hWnd, (HMENU)IDC_MINRAND,
				GetModuleHandleW(NULL), NULL);

			hWndStatic2 = CreateWindowW(L"static", L"Maximum Undercut (ISK-Cent)",
				WS_CHILD | WS_VISIBLE,
				5, 80, 150, 18, hWnd, 0,
				GetModuleHandleW(NULL), NULL);

			hWndMaxRand = CreateWindowW(L"edit", 0,
				WS_CHILD | WS_BORDER | WS_VISIBLE | WS_TABSTOP,
				5, 95, 135, 18, hWnd, (HMENU)IDC_MAXRAND,
				GetModuleHandleW(NULL), NULL);

			hWndOld = CreateWindowW(L"static", L"",
				WS_CHILD | WS_VISIBLE,
				5, 130, 190, 18, hWnd, 0,
				GetModuleHandleW(NULL), NULL);

			hWndNew = CreateWindowW(L"static", L"",
				WS_CHILD | WS_VISIBLE,
				5, 150, 190, 18, hWnd, 0,
				GetModuleHandleW(NULL), NULL);

			defaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			SendMessageW(hWndEnabled,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hWndStatic1,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hWndStatic2,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hWndMinRand,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hWndMaxRand,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hWndOld,			WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hWndNew,			WM_SETFONT, (WPARAM)defaultFont, TRUE);

			PostMessage(hWndEnabled, BM_SETCHECK, BST_CHECKED, 0);

			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_ENABLED:
					switch (HIWORD(wParam)) {
						case BN_CLICKED:
							if (IsDlgButtonChecked(hWnd, IDC_ENABLED)) {
								CheckDlgButton(hWnd, IDC_ENABLED, BST_UNCHECKED);
								progEnabled = FALSE;
							} else {
								CheckDlgButton(hWnd, IDC_ENABLED, BST_CHECKED);
								progEnabled = TRUE;
							}
							break;
					}
					break;
			}
			break;

		case WM_CHANGECBCHAIN:
			// If the next window is closing, repair the chain.
			// Otherwise, pass the message to the next link.
			if ((HWND) wParam == hWndNextViewer) {
				hWndNextViewer = (HWND) lParam;
			} else if (hWndNextViewer != NULL) {
				SendMessage(hWndNextViewer, message, wParam, lParam);
			}
			break;

		case WM_DESTROY:
			ChangeClipboardChain(hWnd, hWndNextViewer);
			PostQuitMessage(0);
			break;

		case WM_DRAWCLIPBOARD:
			if (progEnabled) {
				// Handle the clipboard update in a different message
				// to avoid infinite loops because this program causes
				// clipboard changes itself.
				if (!ignoreChange) {
					PostMessage(hWnd, MSG_CUSTOM_UPDATE, 0, 0);
				}
			}
			if(hWndNextViewer != NULL) {
				SendMessage(hWndNextViewer, message, wParam, lParam);
			}
			break;

		case MSG_CUSTOM_UPDATE:
			OpenClipboard(hWnd);
			hGlobal = GetClipboardData(CF_UNICODETEXT);
			if (hGlobal != NULL) {
				pGlobal = (WCHAR*) GlobalLock(hGlobal);
				if (IsMarketData(pGlobal)) {
					ModifyClipboard(hWnd, pGlobal);
				}
				GlobalUnlock(hGlobal);
			}
			CloseClipboard();
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow) {

	// Create window to catch clipboard messages

	static const char* className = "MarketPasta";
	WNDCLASSEX wx = {};
	wx.cbSize		= sizeof(WNDCLASSEX);
	wx.lpfnWndProc		= WndProc;
	wx.hInstance		= hInstance;
	wx.lpszClassName	= className;

	wx.hbrBackground	= GetSysColorBrush(COLOR_3DFACE);
	wx.hIcon		= LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wx)) {
		Show(L"Failed to register class.\n");
		exit(1);
	}

	hWnd = CreateWindowEx(0, className, "MarketPasta", WS_OVERLAPPEDWINDOW,
		100, 100, 220, 220,
		NULL, NULL, NULL, NULL);
	if (!hWnd) {
		Show(L"Failed to create a window.\n");
		exit(1);
	}

	// Show window
	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	// Handle window messages
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int) msg.wParam;
}