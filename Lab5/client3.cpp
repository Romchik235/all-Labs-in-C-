// client3.cpp

#include <windows.h>
#include <string>
#include <sstream>
#include <cmath>

#define IDC_EDIT_A     1001
#define IDC_EDIT_B     1002
#define IDC_EDIT_H     1003
#define IDC_EDIT_RES   1004
#define IDC_BTN_SEND   1005
#define IDC_BTN_CLEAR  1006

double Area(double a, double b, double h) {
    return (a + b) * h / 2.0;
}

void SendToServer(const std::string& msg) {
    HANDLE h = CreateFileA(
        "\\\\.\\mailslot\\lab5_slot",
        GENERIC_WRITE, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE) return;

    DWORD written;
    WriteFile(h, msg.c_str(), msg.size() + 1, &written, NULL);
    CloseHandle(h);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BTN_SEND) {

            char aBuf[32], bBuf[32], hBuf[32];
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_A), aBuf, 32);
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_B), bBuf, 32);
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_H), hBuf, 32);

            // Перевірка на порожні поля
            if (strlen(aBuf) == 0 || strlen(bBuf) == 0 || strlen(hBuf) == 0) {
                MessageBoxA(hwnd, "Помилка: введіть усі параметри (A, B, H).", "Error", MB_OK | MB_ICONERROR);
                break;
            }

            // Перевірка, що всі символи числові
            auto isNumber = [](const char* str) {
                bool dotFound = false;
                for (int i = 0; str[i]; ++i) {
                    if (str[i] == '.') {
                        if (dotFound) return false;
                        dotFound = true;
                    } else if (str[i] < '0' || str[i] > '9') {
                        return false;
                    }
                }
                return true;
            };

            if (!isNumber(aBuf) || !isNumber(bBuf) || !isNumber(hBuf)) {
                MessageBoxA(hwnd, "Помилка: введіть лише числа для A, B та H.", "Error", MB_OK | MB_ICONERROR);
                break;
            }

            double A = atof(aBuf);
            double B = atof(bBuf);
            double H = atof(hBuf);
            double S = Area(A, B, H);

            // вивід результату у вікні
            char res[64];
            sprintf(res, "Result: %.2f", S);
            SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_RES), res);

            // надсилання даних на сервер
            std::ostringstream ss;
            ss << "Client3: \r\nTrapezoid area = " << S;
            SendToServer(ss.str());
        }


        if (LOWORD(wParam) == IDC_BTN_CLEAR) {
            int r = MessageBoxA(hwnd, "Очистити всі дані?", "Confirm", MB_YESNO);

            if (r == IDYES) {
                SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_A), "");
                SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_B), "");
                SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_H), "");
                SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_RES), "");
            }
        }

        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {

    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "client3_gui";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
        "client3_gui", "Client 3 (trapezoid)",
        WS_OVERLAPPEDWINDOW,
        200, 200, 420, 260,
        NULL, NULL, hInst, NULL);

    // поля вводу
    CreateWindowA("STATIC", "A:", WS_CHILD | WS_VISIBLE,
        20, 20, 20, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        50, 20, 80, 22, hwnd, (HMENU)IDC_EDIT_A, hInst, NULL);

    CreateWindowA("STATIC", "B:", WS_CHILD | WS_VISIBLE,
        20, 60, 20, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        50, 60, 80, 22, hwnd, (HMENU)IDC_EDIT_B, hInst, NULL);

    CreateWindowA("STATIC", "H:", WS_CHILD | WS_VISIBLE,
        20, 100, 20, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        50, 100, 80, 22, hwnd, (HMENU)IDC_EDIT_H, hInst, NULL);

    // поле результату
    CreateWindowA("STATIC", "Result:", WS_CHILD | WS_VISIBLE,
        160, 20, 60, 20, hwnd, NULL, hInst, NULL);

    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
        160, 45, 200, 25, hwnd, (HMENU)IDC_EDIT_RES, hInst, NULL);

    // кнопки
    CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE,
        50, 150, 80, 30, hwnd, (HMENU)IDC_BTN_SEND, hInst, NULL);

    CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE,
        160, 150, 80, 30, hwnd, (HMENU)IDC_BTN_CLEAR, hInst, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
