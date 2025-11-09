#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <winuser.h>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

const int TEXT_SIZE = 32;

typedef struct {
    HWND hwnd;
    DWORD pid;
    wchar_t title[256];
    wchar_t className[256];
} WindowInfo;

#define MAX_WINDOWS 1024
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

// EnumWindows callback to fill g_windowList
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    WindowList* list = (WindowList*)lParam;
    if (list->count >= list->capacity)
        return FALSE; // stop if capacity reached

    if (!IsWindowVisible(hwnd)) return TRUE; // skip invisible

    wchar_t title[256];
    GetWindowTextW(hwnd, title, sizeof(title)/sizeof(title[0]));
    if (wcslen(title) == 0) return TRUE; // skip untitled

    if ((GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD) != 0) return TRUE; // skip child
    if ((GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) return TRUE; // skip tool windows

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

    // Skip UWP Settings window
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

// main
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Window class
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"BorderlessRect";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // Initial dummy size
    int clientW = 1200;
    int clientH = 100;

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, clientW, clientH,
        NULL, NULL, hInstance, NULL
    );
    if (!hwnd) return 0;

    // Start hidden
    ShowWindow(hwnd, SW_HIDE);

    // Register Ctrl+B hotkey
    if (!RegisterHotKey(hwnd, 1, MOD_CONTROL, 'B')) {
        MessageBoxW(NULL, L"Failed to register hotkey Ctrl+B", L"Error", MB_OK);
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
                        if (idx < 0 || idx >= 26) idx = 26-1; // fallback
                        wchar_t secondLetter = L'a' + letterCount[idx];
                        letterCount[idx]++; // increment for next window with same first letter
                        wchar_t expectedId[3] = { firstLetter, secondLetter, L'\0' };
                        if (wcscmp(idBuffer, expectedId) == 0) {
                            // Match found
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
        if (wParam == 1) // Ctrl+B
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

        RECT rect;
        GetClientRect(hwnd, &rect);

        // Draw blue background
        HBRUSH brush = CreateSolidBrush(RGB(5,22,80));
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        HFONT hFont = CreateFontW(
            TEXT_SIZE,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,FF_DONTCARE | DEFAULT_PITCH,L"Consolas"
        );
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255,255,255));

        int iconSize = TEXT_SIZE;
        int iconX = 10;
        int textX = iconX + iconSize + 10;
        int y = 10;
		int letterCount[26] = {0};

        for (int i = 0; i < g_windowList.count; i++)
        {
            WindowInfo* wi = &g_windowList.array[i];

            // get identifier/bind/shortcut
			wchar_t firstLetter = GetFirstLetterOfProcess(wi);
			int idx = firstLetter - L'a';
			if (idx < 0 || idx >= 26) idx = 26-1; // fallback
			wchar_t secondLetter = L'a' + letterCount[idx];
			letterCount[idx]++; // increment for next window with same first letter

			wchar_t id[3] = { firstLetter, secondLetter, L'\0' }; // build 2-letter id

            // Get window icon
            HICON hIcon = (HICON)SendMessageW(wi->hwnd, WM_GETICON, ICON_BIG, 0);
            if (!hIcon) hIcon = (HICON)GetClassLongPtrW(wi->hwnd, GCLP_HICONSM);
            if (!hIcon) hIcon = (HICON)LoadIconW(NULL, IDI_APPLICATION);

            DrawIconEx(hdc, iconX, y, hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);

            wchar_t buf[512];
			StringCchPrintfW(buf, 512, L"%s | %s | %s", id, wi->title, wi->className);
            TextOutW(hdc, textX, y + (iconSize - TEXT_SIZE)/2, buf, lstrlenW(buf));

            y += iconSize + 8;
        }

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        UnregisterHotKey(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

