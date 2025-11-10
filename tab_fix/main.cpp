#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <winuser.h>
#include <shellapi.h>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#define VK_SECTION VK_OEM_3
#define MAX_WINDOWS 1024
#define WM_TRAYICON (WM_USER + 1)

const int TEXT_SIZE = 32;

typedef struct {
    HWND hwnd;
    DWORD pid;
    wchar_t title[256];
    wchar_t className[256];
} WindowInfo;

WindowInfo windows[MAX_WINDOWS];

typedef struct {
    WindowInfo* array;
    int capacity;
    int count;
} WindowList;

WindowList g_windowList = { windows, MAX_WINDOWS, 0 };

// Helper: swapping to window
void SwitchToWindow(HWND hwnd)
{
    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
	SetFocus(hwnd);
}

// Helper: get process name from PID
BOOL GetProcessName(DWORD pid, wchar_t* outName, size_t size)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                StringCchCopyW(outName, size, pe.szExeFile);
                CloseHandle(hSnap);
                return TRUE;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return FALSE;
}

// Helper: get first letter of process name
wchar_t GetFirstLetterOfProcess(WindowInfo* wi)
{
    wchar_t procName[260] = {0};
    if (GetProcessName(wi->pid, procName, 260)) {
        return towlower(procName[0]); // lowercase
    }
    return L'?';
}

// Helper: adding an icon to the tray
void AddTrayIcon(HWND hwnd, HICON hIcon)
{
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = (HICON)LoadImageW(NULL, MAKEINTRESOURCEW(IDI_APPLICATION),
                               IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED);
    StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), L"tab_fix");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd)
{
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

// EnumWindows callback to fill g_windowList
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    WindowList* list = (WindowList*)lParam;
    if (list->count >= list->capacity)
        return FALSE; 

    if (!IsWindowVisible(hwnd)) return TRUE; 

    wchar_t title[256];
    GetWindowTextW(hwnd, title, sizeof(title)/sizeof(title[0]));
    if (wcslen(title) == 0) return TRUE; 

    if ((GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD) != 0) return TRUE; 
    if ((GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) return TRUE; 

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    wchar_t procName[260];
    if (GetProcessName(pid, procName, 260)) {
        if (_wcsicmp(procName, L"SystemSettings.exe") == 0)
            return TRUE; // skip Settings process
    }

    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    if (wcscmp(className, L"Progman") == 0) return TRUE; // skip desktop

    // skip UWP Settings window
    if (wcscmp(className, L"ApplicationFrameWindow") == 0 && wcscmp(title, L"Settings") == 0)
        return TRUE;

    WindowInfo* wi = &list->array[list->count];
    wi->hwnd = hwnd;
    wcscpy_s(wi->title, 256, title);
    wcscpy_s(wi->className, 256, className);
    wi->pid = pid;

    list->count++;
    return TRUE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"BorderlessRect";
    wc.hbrBackground = NULL;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    int clientW = 1200;
    int clientH = 100;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, clientW, clientH,
        NULL, NULL, hInstance, NULL
    );

	HICON hTrayIcon = LoadIconW(NULL, IDI_APPLICATION);
	AddTrayIcon(hwnd, hTrayIcon);

    if (!hwnd) return 0;

    ShowWindow(hwnd, SW_HIDE);

    // hotkey: Ctrl+Space
    if (!RegisterHotKey(hwnd, 1, MOD_CONTROL, VK_SPACE)) {
        MessageBoxW(NULL, L"Failed to register hotkey Ctrl+Space", L"Error", MB_OK);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN: // Esc
        if (wParam == VK_ESCAPE) {
            ShowWindow(hwnd, SW_HIDE);
        }
        else {
            if (IsWindowVisible(hwnd)) {
				static wchar_t idBuffer[3] = { 0 };
                static int idIndex = 0;

                wchar_t c = tolower((wchar_t)wParam);
                if (idIndex < 2) {
                    idBuffer[idIndex++] = c;
                }

                if (idIndex == 2) {
                    int letterCount[26] = { 0 };
                    for (int i = 0; i < g_windowList.count; i++)
                    {
                        WindowInfo* wi = &g_windowList.array[i];
                        wchar_t firstLetter = GetFirstLetterOfProcess(wi);
                        int idx = firstLetter - L'a';
                        if (idx < 0 || idx >= 26) idx = 26-1;
                        wchar_t secondLetter = L'a' + letterCount[idx];
                        letterCount[idx]++;
                        wchar_t expectedId[3] = { firstLetter, secondLetter, L'\0' };
                        if (wcscmp(idBuffer, expectedId) == 0) {
                            SwitchToWindow(wi->hwnd);
                            ShowWindow(hwnd, SW_HIDE);
                            break;
                        }
					}
                    idIndex = 0;
                }

            }
        } break;

    case WM_HOTKEY:
        if (wParam == 1)
        {
            // Refresh window list
            g_windowList.count = 0;
            EnumWindows(EnumWindowsProc, (LPARAM)&g_windowList);

            // Resize based on number of windows
            int clientW = 1200;
            int clientH = 40 * g_windowList.count + 10;

            RECT wr = {0,0,clientW,clientH};
            AdjustWindowRectEx(&wr, WS_POPUP, FALSE, 0);

            int winW = wr.right - wr.left;
            int winH = wr.bottom - wr.top;

            // Center on primary screen
            int x = (GetSystemMetrics(SM_CXSCREEN) - winW)/2;
            int y = (GetSystemMetrics(SM_CYSCREEN) - winH)/2;

            SetWindowPos(hwnd, HWND_TOP, x, y, winW, winH,
                         SWP_SHOWWINDOW | SWP_NOZORDER);

            SetForegroundWindow(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);
        int width = client.right - client.left;
		int height = client.bottom - client.top;

		HDC memDC = CreateCompatibleDC(hdc);
		HBITMAP hBmp = CreateCompatibleBitmap(hdc, width, height);
		HBITMAP hOldBmp = (HBITMAP)SelectObject(memDC, hBmp);


        HBRUSH brush = CreateSolidBrush(RGB(5,22,80));
        FillRect(memDC, &client, brush);
        DeleteObject(brush);

        HFONT hFont = CreateFontW(
            TEXT_SIZE,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,FF_DONTCARE | DEFAULT_PITCH,L"Consolas"
        );
        HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);

        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(255,255,255));

        int iconSize = TEXT_SIZE;
        int iconX = 10;
        int textX = iconX + iconSize + 10;
        int y = 10;
		int letterCount[26] = {0};

        for (int i = 0; i < g_windowList.count; i++)
        {
            WindowInfo* wi = &g_windowList.array[i];

			wchar_t firstLetter = GetFirstLetterOfProcess(wi);
			int idx = firstLetter - L'a';
			if (idx < 0 || idx >= 26) idx = 26-1; 
			wchar_t secondLetter = L'a' + letterCount[idx];
			letterCount[idx]++; 

			wchar_t id[3] = { firstLetter, secondLetter, L'\0' }; 
            
            HICON hIcon = (HICON)SendMessageW(wi->hwnd, WM_GETICON, ICON_BIG, 0);
            if (!hIcon) hIcon = (HICON)GetClassLongPtrW(wi->hwnd, GCLP_HICONSM);
			if (!hIcon) hIcon = (HICON)LoadImageW(NULL, MAKEINTRESOURCEW(IDI_APPLICATION),
                               IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

            DrawIconEx(memDC, iconX, y, hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);

            wchar_t buf[512];
			StringCchPrintfW(buf, 512, L"%s | %s | %s", id, wi->title, wi->className);
            TextOutW(memDC, textX, y + (iconSize - TEXT_SIZE)/2, buf, lstrlenW(buf));

            y += iconSize + 8;
        }

        SelectObject(memDC, hOldFont);
        DeleteObject(hFont);

		BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

		SelectObject(memDC, hOldBmp);
		DeleteObject(hBmp);
		DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
		return 1;

	case WM_TRAYICON:
		if (lParam == WM_RBUTTONUP) {
			POINT pt;
			GetCursorPos(&pt);

			HMENU hMenu = CreatePopupMenu();
			AppendMenuW(hMenu, MF_STRING, 1001, L"Exit");
			SetForegroundWindow(hwnd);
			TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
			DestroyMenu(hMenu);
		}
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == 1001) {
			PostQuitMessage(0);
		}
		break;

		case WM_DESTROY:
			UnregisterHotKey(hwnd, 1);
			PostQuitMessage(0);
			return 0;
		}
    return DefWindowProcW(hwnd, msg, wParam, lParam);

}
