// client1.cpp

#include <windows.h>
#include <string>
#include <sstream>
#include <wininet.h>
#include "common.h"

#pragma comment(lib, "wininet.lib")

#define IDC_CPU       3001
#define IDC_NET       3002
#define IDC_WIDTH     3003
#define IDC_BTN_SEND  3004
#define IDC_BTN_CLR   3005

HANDLE hMapping, hEvent, hExitEvent;
SharedData* shared = nullptr;

// Convert narrow → wide
std::wstring ToW(const std::string& s)
{
    return std::wstring(s.begin(), s.end());
}

// Send data to server
void SendMessageToServer(const std::string& text)
{
    shared->clientId = 1;
    shared->sequence++;
    shared->exitFlag = 0;

    std::wstring w = ToW(text);
    wcscpy_s(shared->message, w.c_str());

    SetEvent(hEvent);
}

// Check for server shutdown
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

        // Send
        if (LOWORD(wp) == IDC_BTN_SEND)
        {
            CheckExit(hwnd);

            char cpu[32], net[32], width[32];
            GetWindowTextA(GetDlgItem(hwnd, IDC_CPU), cpu, 32);
            GetWindowTextA(GetDlgItem(hwnd, IDC_NET), net, 32);
            GetWindowTextA(GetDlgItem(hwnd, IDC_WIDTH), width, 32);

            std::ostringstream ss;
            ss << "CPU cores: " << cpu << "\r\n";
            ss << "Network status: " << net << "\r\n";
            ss << "Screen width: " << width << "\r\n";

            SendMessageToServer(ss.str());
        }

        // Clear fields
        if (LOWORD(wp) == IDC_BTN_CLR)
        {
            SetWindowTextA(GetDlgItem(hwnd, IDC_CPU), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_NET), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_WIDTH), "");
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

    shared = (SharedData*)MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));

    hEvent     = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    hExitEvent = OpenEventW(SYNCHRONIZE, FALSE, EXIT_EVENT);

    // System data
    SYSTEM_INFO sys{};
    GetSystemInfo(&sys);
    int cpuCores = sys.dwNumberOfProcessors;

    DWORD flags = 0;
    BOOL online = InternetGetConnectedState(&flags, 0);
    std::string net = online ? "Connected" : "Offline";

    HDC hdc = GetDC(NULL);
    int width = GetDeviceCaps(hdc, HORZRES);
    ReleaseDC(NULL, hdc);

    // Window class
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client1Window";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
        "Client1Window", "Client 1",
        WS_OVERLAPPEDWINDOW,
        200, 200, 400, 260,
        NULL, NULL, hInst, NULL
    );

    // UI
    CreateWindowA("STATIC", "CPU cores:", WS_CHILD | WS_VISIBLE,
        20, 20, 100, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        130, 20, 200, 22, hwnd, (HMENU)IDC_CPU, hInst, NULL);

    CreateWindowA("STATIC", "Network:", WS_CHILD | WS_VISIBLE,
        20, 60, 100, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        130, 60, 200, 22, hwnd, (HMENU)IDC_NET, hInst, NULL);

    CreateWindowA("STATIC", "Screen width:", WS_CHILD | WS_VISIBLE,
        20, 100, 100, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        130, 100, 200, 22, hwnd, (HMENU)IDC_WIDTH, hInst, NULL);

    CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE,
        70, 150, 100, 30, hwnd, (HMENU)IDC_BTN_SEND, hInst, NULL);
    CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE,
        190, 150, 100, 30, hwnd, (HMENU)IDC_BTN_CLR, hInst, NULL);

    ShowWindow(hwnd, cmdShow);

    // Fill initial system data
    SetWindowTextA(GetDlgItem(hwnd, IDC_CPU), std::to_string(cpuCores).c_str());
    SetWindowTextA(GetDlgItem(hwnd, IDC_NET), net.c_str());
    SetWindowTextA(GetDlgItem(hwnd, IDC_WIDTH), std::to_string(width).c_str());

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        CheckExit(hwnd);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
