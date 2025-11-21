// client1.cpp 

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

#ifndef SM_POWERBUTTONS
#define SM_POWERBUTTONS 59
#endif

int batteryState = GetSystemMetrics(SM_POWERBUTTONS);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    const int WIDTH = 480;
    const int HEIGHT = 360;

    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int mouseButtons = GetSystemMetrics(SM_CMOUSEBUTTONS);
    int borders = GetSystemMetrics(SM_CXFRAME);


    int cleanBoot = GetSystemMetrics(SM_CLEANBOOT);
    // int antivirusState = GetSystemMetrics(SM_CXMIN);
    int antivirusState = 0; // стан антивірусу неможливо отримати через стандартний API
    int batteryState = GetSystemMetrics(SM_POWERBUTTONS);

    HDC hdc = GetDC(NULL);
    int horzRes = GetDeviceCaps(hdc, HORZRES); // ширина екрана
    ReleaseDC(NULL, hdc);

    
    std::ostringstream ss;
    ss << "Client1:\r\n";

   
    ss << "Screen width = " << screenWidth << "\r\n";
    ss << "Screen height = " << screenHeight << "\r\n";
    ss << "Mouse buttons = " << mouseButtons << "\r\n";
    ss << "Window border width = " << borders << "\r\n";
    ss << "Clean boot (SM_CLEANBOOT) = " << cleanBoot << "\r\n";
    ss << "Antivirus state (simulated) = " << antivirusState << "\r\n";
    ss << "Screen HORZRES = " << horzRes << "\r\n";
    ss << "Battery/power state (SM_POWERBUTTONS) = " << batteryState << "\r\n";

    std::string message = ss.str();

    // === GUI ===
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client1Class";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
        "Client1Class",
        "Client 1 info",
        WS_OVERLAPPEDWINDOW,
        200, 200,
        WIDTH, HEIGHT,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SetWindowTextA(hEditOut, message.c_str());

    SendToServer(message);

    MessageBoxA(hwnd, "Client1 data sent to server!", "OK", MB_OK);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
