// client2.cpp

#include <windows.h>
#include <string>
#include <sstream>
#include "common.h"

#define IDC_MENUFONT   4001
#define IDC_BITDEPTH   4002
#define IDC_COLORS     4003
#define IDC_BTN_SEND   4004
#define IDC_BTN_CLEAR  4005

HANDLE hMapping, hEvent, hExitEvent;
SharedData* shared = nullptr;

// Convert std::string -> std::wstring
std::wstring ToW(const std::string& s)
{
    return std::wstring(s.begin(), s.end());
}

// Send formatted text to server
void SendToServer(const std::string& text)
{
    shared->clientId = 2;
    shared->sequence++;
    shared->exitFlag = 0;

    std::wstring w = ToW(text);
    wcscpy_s(shared->message, w.c_str());
    SetEvent(hEvent);
}

// Check if server requested shutdown
void CheckExit(HWND hwnd)
{
    if (shared->exitFlag == 1)
    {
        MessageBoxA(hwnd, "Server closed.", "Exit", MB_OK);
        PostQuitMessage(0);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:

        // SEND button
        if (LOWORD(wp) == IDC_BTN_SEND)
        {
            CheckExit(hwnd);

            char a[64], b[64], c[64];
            GetWindowTextA(GetDlgItem(hwnd, IDC_MENUFONT),  a, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_BITDEPTH),  b, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_COLORS),    c, 64);

            std::ostringstream ss;
            ss << "Menu font height: " << a << "\r\n";
            ss << "Bit depth: "        << b << "\r\n";
            ss << "Colors available: " << c << "\r\n";

            SendToServer(ss.str());
        }

        // CLEAR button
        if (LOWORD(wp) == IDC_BTN_CLEAR)
        {
            SetWindowTextA(GetDlgItem(hwnd, IDC_MENUFONT), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_BITDEPTH), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_COLORS),   "");
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int cmdShow)
{
    // Connect to shared memory
    hMapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, MAPPING_NAME);
    if (!hMapping)
    {
        MessageBoxA(NULL, "Server not running!", "Error", MB_OK);
        return 1;
    }

    shared = (SharedData*) MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    hEvent     = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    hExitEvent = OpenEventW(SYNCHRONIZE, FALSE, EXIT_EVENT);

    // -------- SYSTEM METRICS --------
    int menuFont = GetSystemMetrics(SM_CYMENU);

    HDC hdc = GetDC(NULL);
    int bitDepth  = GetDeviceCaps(hdc, BITSPIXEL);
    int rawColors = GetDeviceCaps(hdc, NUMCOLORS);
    ReleaseDC(NULL, hdc);

    // Convert -1 to real color count text
    std::string colors;
    if (rawColors == -1)
        colors = "True color";      // final displayed value
    else
        colors = std::to_string(rawColors);

    // -------- GUI WINDOW --------
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance   = hInst;
    wc.lpszClassName = "Client2Window";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
        "Client2Window", "Client 2",
        WS_OVERLAPPEDWINDOW,
        300, 300, 420, 260,
        NULL, NULL, hInst, NULL
    );

    // -------- UI CONTROLS --------
    CreateWindowA("STATIC", "Menu font height:", WS_CHILD | WS_VISIBLE,
        20, 20, 130, 20, hwnd, NULL, hInst, NULL);

    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        160, 20, 180, 22, hwnd, (HMENU)IDC_MENUFONT, hInst, NULL);

    CreateWindowA("STATIC", "Bit depth:", WS_CHILD | WS_VISIBLE,
        20, 60, 130, 20, hwnd, NULL, hInst, NULL);

    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        160, 60, 180, 22, hwnd, (HMENU)IDC_BITDEPTH, hInst, NULL);

    CreateWindowA("STATIC", "Colors:", WS_CHILD | WS_VISIBLE,
        20, 100, 130, 20, hwnd, NULL, hInst, NULL);

    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        160, 100, 180, 22, hwnd, (HMENU)IDC_COLORS, hInst, NULL);

    CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE,
        60, 150, 120, 30, hwnd, (HMENU)IDC_BTN_SEND, hInst, NULL);

    CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE,
        200, 150, 120, 30, hwnd, (HMENU)IDC_BTN_CLEAR, hInst, NULL);

    ShowWindow(hwnd, cmdShow);

    // -------- Prefill data --------
    SetWindowTextA(GetDlgItem(hwnd, IDC_MENUFONT), std::to_string(menuFont).c_str());
    SetWindowTextA(GetDlgItem(hwnd, IDC_BITDEPTH), std::to_string(bitDepth).c_str());
    SetWindowTextA(GetDlgItem(hwnd, IDC_COLORS),   colors.c_str());

    // -------- Message loop --------
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        CheckExit(hwnd);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
