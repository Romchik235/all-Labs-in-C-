// server.cpp
// WinAPI GUI server that accepts connections from clients and appends received text to an Edit control.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <sstream>
#pragma comment(lib, "ws2_32.lib")

#define PORT 5000
#define BUF_SIZE 1024

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ServerWindowClass";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Server (port 5000)", WS_OVERLAPPEDWINDOW,
        100, 100, 520, 420, nullptr, nullptr, hInstance, nullptr);

    hEdit = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        10, 10, 480, 340, hwnd, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBoxA(NULL, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        MessageBoxA(NULL, "socket() failed", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        MessageBoxA(NULL, "bind() failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        MessageBoxA(NULL, "listen() failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    AppendText("=== SERVER RUNNING ON PORT 5000 ===\r\n");

    u_long nonblock = 1;
    ioctlsocket(serverSocket, FIONBIO, &nonblock);

    MSG msg = {};
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        sockaddr_in clientAddr{};
        int clientSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        if (clientSocket != INVALID_SOCKET) {
            u_long nb = 1;
            ioctlsocket(clientSocket, FIONBIO, &nb);

            char buf[BUF_SIZE];
            std::stringstream ss;
            // show connection header with IP
            ss << "=== Connection from client (" << inet_ntoa(clientAddr.sin_addr) << ") ===\r\n";

            std::string received;
            while (true) {
                int r = recv(clientSocket, buf, sizeof(buf) - 1, 0);
                if (r > 0) {
                    buf[r] = 0;
                    received.append(buf);
                    // allow more data to arrive
                    Sleep(10);
                    continue;
                } else {
                    break;
                }
            }

            if (!received.empty()) {
                // if client already sends a "Client..." prefix, keep it.
                if (received.find("Client") == std::string::npos) {
                    ss << "Client 3:\r\n";
                    ss << received << "\r\n\r\n";
                } else {
                    ss << received << "\r\n\r\n";
                }
            } else {
                ss << "(no data received)\r\n\r\n";
            }

            AppendText(ss.str());

            closesocket(clientSocket);
        }

        Sleep(50);
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
