// client1.cpp

#include <windows.h>
#include <string>
#include <sstream>
#include "common.h"

#define IDC_RAM      3001
#define IDC_DISK     3002
#define IDC_WIDTH    3003
#define IDC_BTN_SEND 3004
#define IDC_BTN_CLR  3005

HANDLE hMapping, hEvent, hExitEvent;
SharedData* shared = nullptr;

// Convert char* to wstring
std::wstring ToW(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

void SendMessageToServer(std::string text)
{
    shared->clientId = 1;
    shared->sequence++;
    shared->exitFlag = 0;

    std::wstring w = ToW(text);
    wcscpy_s(shared->message, w.c_str());

    SetEvent(hEvent);
}

void CheckExit(HWND hwnd)
{
    if (shared->exitFlag == 1) {
        MessageBoxA(hwnd, "Сервер завершив роботу.", "Exit", MB_OK);
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
            CheckExit(hwnd);

            char ramBuf[64], diskBuf[64], widthBuf[64];

            GetWindowTextA(GetDlgItem(hwnd, IDC_RAM), ramBuf, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_DISK), diskBuf, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_WIDTH), widthBuf, 64);

            std::ostringstream ss;
            ss << "RAM total: " << ramBuf << " MB\r\n";
            ss << "External disk: " << diskBuf << "\r\n";
            ss << "Screen width: " << widthBuf << "\r\n";

            SendMessageToServer(ss.str());
        }

        if (LOWORD(wp) == IDC_BTN_CLR)
        {
            SetWindowTextA(GetDlgItem(hwnd, IDC_RAM), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_DISK), "");
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
    // OPEN FILEMAPPING
    hMapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, MAPPING_NAME);
    if (!hMapping) {
        MessageBoxA(NULL, "Server not running!", "Error", MB_OK);
        return 1;
    }

    shared = (SharedData*)MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    hExitEvent = OpenEventW(SYNCHRONIZE, FALSE, EXIT_EVENT);

    // GET SYSTEM DATA
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);

    bool externalDisk = (GetDriveTypeA("D:\\") == DRIVE_REMOVABLE ||
                         GetDriveTypeA("E:\\") == DRIVE_REMOVABLE);

    HDC hdc = GetDC(NULL);
    int width = GetDeviceCaps(hdc, HORZRES);
    ReleaseDC(NULL, hdc);

    // GUI CLASS
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client1Window";
    RegisterClassA(&wc);

    // WINDOW
    HWND hwnd = CreateWindowA(
        "Client1Window", "Client 1",
        WS_OVERLAPPEDWINDOW,
        300, 200, 400, 270,
        NULL, NULL, hInst, NULL);

    // CONTROLS
    CreateWindowA("STATIC", "RAM:", WS_CHILD | WS_VISIBLE, 20, 20, 80, 20, hwnd, 0, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                  120, 20, 200, 22, hwnd, (HMENU)IDC_RAM, hInst, NULL);

    CreateWindowA("STATIC", "External Disk:", WS_CHILD | WS_VISIBLE, 20, 60, 100, 20, hwnd, 0, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                  120, 60, 200, 22, hwnd, (HMENU)IDC_DISK, hInst, NULL);

    CreateWindowA("STATIC", "Screen width:", WS_CHILD | WS_VISIBLE, 20, 100, 100, 20, hwnd, 0, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                  120, 100, 200, 22, hwnd, (HMENU)IDC_WIDTH, hInst, NULL);

    // BUTTONS
    CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE,
                  70, 150, 100, 30, hwnd, (HMENU)IDC_BTN_SEND, hInst, NULL);

    CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE,
                  190, 150, 100, 30, hwnd, (HMENU)IDC_BTN_CLR, hInst, NULL);

    // SHOW WINDOW
    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    // FILL TEXTBOXES WITH SYSTEM DATA
    std::string ramStr = std::to_string(mem.ullTotalPhys / (1024 * 1024));
    std::string diskStr = externalDisk ? "YES" : "NO";
    std::string widthStr = std::to_string(width);

    SetWindowTextA(GetDlgItem(hwnd, IDC_RAM), ramStr.c_str());
    SetWindowTextA(GetDlgItem(hwnd, IDC_DISK), diskStr.c_str());
    SetWindowTextA(GetDlgItem(hwnd, IDC_WIDTH), widthStr.c_str());

    // MAIN LOOP
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        CheckExit(hwnd);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
