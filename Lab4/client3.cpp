// client3.cpp
// GUI client: user types arbitrary text and presses Send. Sent text is shown in the client's Edit and sent to server.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <sstream>
#pragma comment(lib, "ws2_32.lib")

#define PORT 5000
#define IDC_EDIT_INPUT 1001
#define IDC_BTN_SEND  1002
#define IDC_EDIT_LOG  1003

HWND hEditLog;
HWND hEditInput;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_SEND) {
            int len = GetWindowTextLengthA(hEditInput);
            if (len > 0) {
                std::string text;
                text.resize(len + 1);
                GetWindowTextA(hEditInput, &text[0], len + 1);
                // remove trailing null used in resize
                text.resize(len);

                // Show in local log
                std::string show = "Client3 (you):\r\n" + text + "\r\n\r\n";
                int pos = GetWindowTextLengthA(hEditLog);
                SendMessageA(hEditLog, EM_SETSEL, pos, pos);
                SendMessageA(hEditLog, EM_REPLACESEL, FALSE, (LPARAM)show.c_str());

                // Send to server
                WSADATA wsa;
                if (WSAStartup(MAKEWORD(2,2), &wsa) == 0) {
                    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (sock != INVALID_SOCKET) {
                        sockaddr_in server{};
                        server.sin_family = AF_INET;
                        server.sin_port = htons(PORT);
                        inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
                        if (connect(sock, (sockaddr*)&server, sizeof(server)) == 0) {
                            send(sock, text.c_str(), (int)text.size(), 0);
                        }
                        closesocket(sock);
                    }
                    WSACleanup();
                }

                // clear input
                SetWindowTextA(hEditInput, "");
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client3Window";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Client3 (send text)", WS_OVERLAPPEDWINDOW,
        1100, 100, 480, 340, nullptr, nullptr, hInst, nullptr);

    hEditInput = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        10, 10, 350, 24, hwnd, (HMENU)IDC_EDIT_INPUT, hInst, nullptr);

    HWND hBtn = CreateWindowExA(0, "BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        370, 8, 80, 28, hwnd, (HMENU)IDC_BTN_SEND, hInst, nullptr);

    hEditLog = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        10, 48, 440, 240, hwnd, (HMENU)IDC_EDIT_LOG, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
