// client2.cpp

#include <windows.h>
#include <string>
#include <sstream>
#include "common.h"

#define IDC_CYEDGE     4001
#define IDC_CXEDGE     4002
#define IDC_DPI        4003
#define IDC_BTN_SEND   4004
#define IDC_BTN_CLEAR  4005

HANDLE hMapping, hEvent, hExitEvent;
SharedData* shared = nullptr;

std::wstring ToW(const std::string& s)
{
    return std::wstring(s.begin(), s.end());
}

void SendToServer(std::string text)
{
    shared->clientId = 2;
    shared->sequence++;
    shared->exitFlag = 0;

    std::wstring w = ToW(text);
    wcscpy_s(shared->message, w.c_str());

    SetEvent(hEvent);
}

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
    switch(msg)
    {
    case WM_COMMAND:

        if (LOWORD(wp) == IDC_BTN_SEND)
        {
            char a[64], b[64], c[64];
            GetWindowTextA(GetDlgItem(hwnd, IDC_CYEDGE), a, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_CXEDGE), b, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_DPI), c, 64);

            std::ostringstream ss;
            ss << "Status line height: " << a << "\r\n";
            ss << "Notification panel width: " << b << "\r\n";
            ss << "DPI (X): " << c << "\r\n";

            SendToServer(ss.str());
        }

        if (LOWORD(wp) == IDC_BTN_CLEAR)
        {
            SetWindowTextA(GetDlgItem(hwnd, IDC_CYEDGE), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_CXEDGE), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_DPI), "");
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
    // FILEMAPPING
    hMapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, MAPPING_NAME);
    if (!hMapping) {
        MessageBoxA(NULL, "Server not running!", "Error", MB_OK);
        return 1;
    }

    shared = (SharedData*)MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    hExitEvent = OpenEventW(SYNCHRONIZE, FALSE, EXIT_EVENT);

    // SYSTEM DATA
    int cyEdge = GetSystemMetrics(SM_CYEDGE);
    int cxEdge = GetSystemMetrics(SM_CXEDGE);

    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);

    // GUI CLASS
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client2Window";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
        "Client2Window", "Client 2",
        WS_OVERLAPPEDWINDOW,
        350, 220, 420, 260,
        NULL, NULL, hInst, NULL);

    // STATIC + EDIT
    CreateWindowA("STATIC", "CYEDGE:", WS_CHILD | WS_VISIBLE,
        20, 20, 80, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        120, 20, 180, 22, hwnd, (HMENU)IDC_CYEDGE, hInst, NULL);

    CreateWindowA("STATIC", "CXEDGE:", WS_CHILD | WS_VISIBLE,
        20, 60, 80, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        120, 60, 180, 22, hwnd, (HMENU)IDC_CXEDGE, hInst, NULL);

    CreateWindowA("STATIC", "DPI(X):", WS_CHILD | WS_VISIBLE,
        20, 100, 80, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        120, 100, 180, 22, hwnd, (HMENU)IDC_DPI, hInst, NULL);

    // BUTTONS
    CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE,
                  70, 150, 100, 30, hwnd, (HMENU)IDC_BTN_SEND, hInst, NULL);

    CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE,
                  190, 150, 100, 30, hwnd, (HMENU)IDC_BTN_CLEAR, hInst, NULL);

    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    // SET SYSTEM VALUES
    SetWindowTextA(GetDlgItem(hwnd, IDC_CYEDGE), std::to_string(cyEdge).c_str());
    SetWindowTextA(GetDlgItem(hwnd, IDC_CXEDGE), std::to_string(cxEdge).c_str());
    SetWindowTextA(GetDlgItem(hwnd, IDC_DPI), std::to_string(dpi).c_str());

    // LOOP
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        CheckExit(hwnd);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
