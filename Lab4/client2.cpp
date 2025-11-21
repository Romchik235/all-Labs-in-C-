// client2.cpp
// Shows a window and sends: MenuButtonHeight (SM_CYBUTTON), StartButtonWidth (SM_CXMINTRACK), ScreenLines (VERTRES)

#define _WIN32_WINNT 0x0600
#define WINVER 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winuser.h>
#include <string>
#include <sstream>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")

#ifndef SM_CYBUTTON
#define SM_CYBUTTON 23
#endif

#ifndef SM_CXMINTRACK
#define SM_CXMINTRACK 34
#endif

#define PORT 5000

HWND hEdit;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void AppendText(const std::string& text) {
    int len = GetWindowTextLengthA(hEdit);
    SendMessageA(hEdit, EM_SETSEL, len, len);
    SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

std::string GatherClient2Info() {
    int menuButtonHeight = GetSystemMetrics(SM_CYBUTTON);
    int startButtonWidth = GetSystemMetrics(SM_CXMINTRACK);
    HDC hdc = GetDC(NULL);
    int lines = GetDeviceCaps(hdc, VERTRES);
    ReleaseDC(NULL, hdc);

    std::ostringstream ss;
    ss << "Client2:\r\n";
    ss << "MenuButtonHeight (SM_CYBUTTON): " << menuButtonHeight << "\r\n";
    ss << "StartButtonWidth (SM_CXMINTRACK): " << startButtonWidth << "\r\n";
    ss << "ScreenLines (VERTRES): " << lines << "\r\n";
    return ss.str();
}

void SendToServer(const std::string& text) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return; }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(sock, (sockaddr*)&server, sizeof(server)) == 0) {
        send(sock, text.c_str(), (int)text.size(), 0);
    }

    closesocket(sock);
    WSACleanup();
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client2Window";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Client2", WS_OVERLAPPEDWINDOW,
        650, 420, 420, 300, nullptr, nullptr, hInst, nullptr);

    hEdit = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        10, 10, 380, 230, hwnd, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    std::string info = GatherClient2Info();
    AppendText("=== Data to send ===\r\n");
    AppendText(info);
    AppendText("\r\n");

    SendToServer(info);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
