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

// Convert std::string → std::wstring
std::wstring ToW(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

// Send final message to server
void SendToServer(const std::string& text)
{
    shared->clientId = 3;
    shared->sequence++;
    shared->exitFlag = 0;

    std::wstring w = ToW(text);
    wcscpy_s(shared->message, w.c_str());
    SetEvent(hEvent);
}

// Validate positive numbers (≥ 0)
bool IsPositiveNumber(const char* s)
{
    bool dot = false;
    int i = 0;

    // allow leading spaces
    while (s[i] == ' ') i++;

    if (!s[i]) return false;

    for (; s[i]; i++)
    {
        if (s[i] == '.')
        {
            if (dot) return false;
            dot = true;
            continue;
        }
        if (s[i] < '0' || s[i] > '9')
            return false;
    }
    return true;
}

// Trapezoid area
double Area(double a, double b, double h)
{
    return (a + b) * h / 2.0;
}

// Check exit signal from server
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

        if (LOWORD(wp) == IDC_BTN_SEND)
        {
            CheckExit(hwnd);

            char a[32], b[32], h[32], txt[512];
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_A), a, 32);
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_B), b, 32);
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_H), h, 32);
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_TEXT), txt, 512);

            bool hasTrap = strlen(a) || strlen(b) || strlen(h);
            bool hasText = strlen(txt) > 0;

            if (!hasTrap && !hasText)
            {
                MessageBoxA(hwnd, "No data to send!", "Error", MB_OK | MB_ICONERROR);
                return 0;
            }

            double A = 0, B = 0, H = 0, S = 0;

            // Trapezoid parameters
            if (hasTrap)
            {
                if (!IsPositiveNumber(a) || !IsPositiveNumber(b) || !IsPositiveNumber(h))
                {
                    MessageBoxA(hwnd, "A, B and H must be non-negative numbers!", "Error", MB_OK | MB_ICONERROR);
                    return 0;
                }

                A = atof(a);
                B = atof(b);
                H = atof(h);

                if (A < 0 || B < 0 || H < 0)
                {
                    MessageBoxA(hwnd, "Values cannot be negative!", "Error", MB_OK | MB_ICONERROR);
                    return 0;
                }

                S = Area(A, B, H);

                // char res[64];
                char res[70];
                sprintf(res, "%.2f", S);   
                SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_RES), res);

            }

            // Build message for server
            std::ostringstream out;
            out << "Client3:\r\n";

            if (hasTrap)
            {
                out << "Trapezoid parameters (sides and height):\r\n";
                out << "A = " << A << "\r\n";
                out << "B = " << B << "\r\n";
                out << "H = " << H << "\r\n";
                out << "Area of the trapezoid = " << S << "\r\n\r\n";
            }

            if (hasText)
            {
                out << "Text:\r\n" << txt << "\r\n";
            }

            SendToServer(out.str());
            MessageBoxA(hwnd, "Data sent!", "Info", MB_OK);
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
    // Connect to shared memory
    hMapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, MAPPING_NAME);
    if (!hMapping)
    {
        MessageBoxA(NULL, "Server not running!", "Error", MB_OK);
        return 1;
    }

    shared = (SharedData*)MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    hExitEvent = OpenEventW(SYNCHRONIZE, FALSE, EXIT_EVENT);

    // Window class
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "Client3Window";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
        "Client3Window", "Client 3",
        WS_OVERLAPPEDWINDOW,
        300, 200, 520, 380,
        NULL, NULL, hInst, NULL
    );

    // UI layout
    CreateWindowA("STATIC", "A:", WS_CHILD | WS_VISIBLE,
        20, 20, 30, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        60, 20, 100, 22, hwnd, (HMENU)IDC_EDIT_A, hInst, NULL);

    CreateWindowA("STATIC", "B:", WS_CHILD | WS_VISIBLE,
        20, 55, 30, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        60, 55, 100, 22, hwnd, (HMENU)IDC_EDIT_B, hInst, NULL);

    CreateWindowA("STATIC", "H:", WS_CHILD | WS_VISIBLE,
        20, 90, 30, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        60, 90, 100, 22, hwnd, (HMENU)IDC_EDIT_H, hInst, NULL);

    // Text field (smaller height)
    CreateWindowA("STATIC", "Text:", WS_CHILD | WS_VISIBLE,
        200, 20, 60, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
        200, 45, 280, 107, hwnd, (HMENU)IDC_EDIT_TEXT, hInst, NULL);


    // Result below label
    CreateWindowA("STATIC", "Area of the trapezoid:", WS_CHILD | WS_VISIBLE,
        20, 140, 160, 20, hwnd, NULL, hInst, NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
        20, 165, 240, 22, hwnd, (HMENU)IDC_EDIT_RES, hInst, NULL);

    CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE,
        120, 250, 120, 35, hwnd, (HMENU)IDC_BTN_SEND, hInst, NULL);

    CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE,
        260, 250, 120, 35, hwnd, (HMENU)IDC_BTN_CLEAR, hInst, NULL);

    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        CheckExit(hwnd);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
