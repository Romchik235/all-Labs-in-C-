// client1.cpp
// Shows a window and sends: time, CleanBoot, AntiVirus(state), ScreenWidth

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#pragma comment(lib, "ws2_32.lib")

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

std::string GatherClient1Info() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    int bootMode = GetSystemMetrics(SM_CLEANBOOT);
    int antiVirus = GetSystemMetrics(SM_CXMIN); // user used SM_CXMIN as "antivirus" placeholder
    HDC hdc = GetDC(NULL);
    int width = GetDeviceCaps(hdc, HORZRES);
    ReleaseDC(NULL, hdc);

    std::ostringstream ss;
    ss << "Client1:\r\n";
    ss << "Time: " << std::setw(2) << std::setfill('0') << st.wHour << ":"
       << std::setw(2) << std::setfill('0') << st.wMinute << ":"
       << std::setw(2) << std::setfill('0') << st.wSecond << "\r\n";
    ss << "CleanBoot (SM_CLEANBOOT): " << bootMode << "\r\n";
    ss << "AntiVirus (SM_CXMIN - used as placeholder): " << antiVirus << "\r\n";
    ss << "ScreenWidth (HORZRES): " << width << "\r\n";
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
    wc.lpszClassName = "Client1Window";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Client1", WS_OVERLAPPEDWINDOW,
        650, 100, 420, 300, nullptr, nullptr, hInst, nullptr);

    hEdit = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        10, 10, 380, 230, hwnd, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // gather info, show locally, and send
    std::string info = GatherClient1Info();
    AppendText("=== Data to send ===\r\n");
    AppendText(info);
    AppendText("\r\n");

    // send asynchronously (not to block UI) -- do it quickly here; if server not available, just continue
    SendToServer(info);

    // simple message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
