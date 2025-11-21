// client2.cpp

#include <windows.h>
#include <string>
#include <sstream>

#define IDC_EDIT_OUT 2001
HWND hEditOut;

void SendToServer(const std::string& text) {
    HANDLE h = CreateFileA(
        "\\\\.\\mailslot\\lab5_slot",
        GENERIC_WRITE, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) return;

    DWORD written;
    WriteFile(h, text.c_str(), text.size() + 1, &written, NULL);
    CloseHandle(h);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        hEditOut = CreateWindowA(
            "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
            20, 20, 380, 220,
            hwnd, (HMENU)IDC_EDIT_OUT, NULL, NULL);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

#ifndef SM_CYBUTTON
#define SM_CYBUTTON 15
#endif

int menuButtonHeight = GetSystemMetrics(SM_CYBUTTON);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    const int WIDTH = 480;
    const int HEIGHT = 360;

    int minHeight = GetSystemMetrics(SM_CYMIN);
    int minWidth = GetSystemMetrics(SM_CXMIN);
    int menus = GetSystemMetrics(SM_CYMENU);


    int menuButtonHeight = GetSystemMetrics(SM_CYBUTTON);
    int startButtonWidth = GetSystemMetrics(SM_CXMINTRACK);

    HDC hdc = GetDC(NULL);
    int vertRes = GetDeviceCaps(hdc, VERTRES); // кількість рядків/висота екрану
    ReleaseDC(NULL, hdc);

    std::ostringstream ss;
    ss << "Client2:\r\n";
    ss << "Min window height = " << minHeight << "\r\n";
    ss << "Min window width = " << minWidth << "\r\n";
    ss << "Menu height = " << menus << "\r\n";
    
    ss << "Menu button height (SM_CYBUTTON) = " << menuButtonHeight << "\r\n";
    ss << "Start button width (SM_CXMINTRACK) = " << startButtonWidth << "\r\n";
    ss << "Screen height (VERTRES) = " << vertRes << "\r\n";

    std::string message = ss.str();

    // GUI
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client2Class";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
        "Client2Class",
        "Client 2 info",
        WS_OVERLAPPEDWINDOW,
        250, 250,
        WIDTH, HEIGHT,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SetWindowTextA(hEditOut, message.c_str());

    SendToServer(message);

    MessageBoxA(hwnd, "Client2 data sent to server!", "OK", MB_OK);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
