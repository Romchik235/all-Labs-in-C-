// client3.cpp

#include <windows.h>
#include <string>
#include <sstream>
#include "common.h"

#define IDC_EDIT_A     5001
#define IDC_EDIT_B     5002
#define IDC_EDIT_H     5003
#define IDC_EDIT_RES   5004
#define IDC_EDIT_TEXT  5005
#define IDC_BTN_SEND   5006
#define IDC_BTN_CLEAR  5007

HANDLE hMapping, hEvent, hExitEvent;
SharedData* shared = nullptr;

std::wstring ToW(const std::string& s)
{
    return std::wstring(s.begin(), s.end());
}

void SendToServer(const std::string& text, int clientId = 3)
{
    shared->clientId = clientId;
    shared->sequence++;
    shared->exitFlag = 0;

    std::wstring w = ToW(text);
    wcscpy_s(shared->message, w.c_str());
    SetEvent(hEvent);
}

bool IsNumber(const char* p)
{
    bool dot = false;
    for (int i = 0; p[i]; i++)
    {
        if (p[i] == '.')
        {
            if (dot) return false;
            dot = true;
        }
        else if (p[i] < '0' || p[i] > '9')
            return false;
    }
    return true;
}

double Area(double a, double b, double h)
{
    return (a + b) * h / 2.0;
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
            CheckExit(hwnd);

            char a[64], b[64], h[64], txt[512];
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_A), a, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_B), b, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_H), h, 64);
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_TEXT), txt, 512);

            bool hasTrap = strlen(a) || strlen(b) || strlen(h);
            bool hasText = strlen(txt);

            double A = 0, B = 0, H = 0, S = 0;

            // --- Перевірка та обчислення трапеції ---
            if (hasTrap)
            {
                if (!strlen(a) || !strlen(b) || !strlen(h))
                {
                    MessageBoxA(hwnd, "Введіть A, B і H повністю!", "Error", MB_OK | MB_ICONERROR);
                    break;
                }
                if (!IsNumber(a) || !IsNumber(b) || !IsNumber(h))
                {
                    MessageBoxA(hwnd, "A, B і H повинні бути числами!", "Error", MB_OK | MB_ICONERROR);
                    break;
                }

                A = atof(a);
                B = atof(b);
                H = atof(h);
                S = Area(A, B, H);

                char res[64];
                sprintf(res, "Result: %.2f", S);
                SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_RES), res);
            }

            if (!hasTrap && !hasText)
            {
                MessageBoxA(hwnd, "Немає даних для відправлення!", "Error", MB_OK | MB_ICONERROR);
                break;
            }

            // === Формуємо фінальне повідомлення (ОДНЕ) ===
            std::ostringstream finalMsg;
            finalMsg << "Client3:\r\n";

            if (hasTrap)
            {
                finalMsg << "A = " << A << "\r\n";
                finalMsg << "B = " << B << "\r\n";
                finalMsg << "H = " << H << "\r\n";
                finalMsg << "Result = " << S << "\r\n";
            }

            if (hasText)
            {
                finalMsg << "\r\nText:\r\n";
                finalMsg << txt << "\r\n";
            }

            // --- Надсилання єдиного блоку ---
            SendToServer(finalMsg.str());
            MessageBoxA(hwnd, "Дані надіслано!", "Info", MB_OK);

            break;
        }

        if (LOWORD(wp) == IDC_BTN_CLEAR)
        {
            SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_A), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_B), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_H), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_RES), "");
            SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_TEXT), "");
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
    // FILEMAPPING connect
    hMapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, MAPPING_NAME);
    if (!hMapping)
    {
        MessageBoxA(NULL, "Server not running!", "Error", MB_OK);
        return 1;
    }

    shared = (SharedData*)MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    hExitEvent = OpenEventW(SYNCHRONIZE, FALSE, EXIT_EVENT);

    // GUI CLASS
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client3Window";
    RegisterClassA(&wc);

    // WINDOW
    HWND hwnd = CreateWindowA(
        "Client3Window", "Client 3",
        WS_OVERLAPPEDWINDOW,
        300, 200, 520, 400,
        NULL, NULL, hInst, NULL);

    // --- UI FIELDS ---
    CreateWindowA("STATIC", "A:", WS_CHILD | WS_VISIBLE, 20, 20, 20, 20, hwnd, 0, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                   50, 20, 100, 22, hwnd, (HMENU)IDC_EDIT_A, hInst, NULL);

    CreateWindowA("STATIC", "B:", WS_CHILD | WS_VISIBLE, 20, 60, 20, 20, hwnd, 0, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                   50, 60, 100, 22, hwnd, (HMENU)IDC_EDIT_B, hInst, NULL);

    CreateWindowA("STATIC", "H:", WS_CHILD | WS_VISIBLE, 20, 100, 20, 20, hwnd, 0, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                   50, 100, 100, 22, hwnd, (HMENU)IDC_EDIT_H, hInst, NULL);

    CreateWindowA("STATIC", "Result:", WS_CHILD | WS_VISIBLE,
                  180, 20, 60, 20, hwnd, 0, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                  250, 20, 200, 25, hwnd, (HMENU)IDC_EDIT_RES, hInst, NULL);

    CreateWindowA("STATIC", "Text:", WS_CHILD | WS_VISIBLE,
                  180, 60, 60, 20, hwnd, 0, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
                  180, 85, 300, 160, hwnd, (HMENU)IDC_EDIT_TEXT, hInst, NULL);

    // --- BUTTONS ---
    CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE,
                  120, 270, 120, 35, hwnd, (HMENU)IDC_BTN_SEND, hInst, NULL);

    CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE,
                  270, 270, 120, 35, hwnd, (HMENU)IDC_BTN_CLEAR, hInst, NULL);

    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

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
