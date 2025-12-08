// server.cpp

#include <windows.h>
#include <string>
#include <sstream>
#include "common.h"

#define IDC_EDIT_LOG   2001
#define IDC_BTN_CLOSE  2002

HWND hEdit;
HANDLE hMapping, hEvent, hExitEvent;
SharedData* shared = nullptr;

void AppendText(const std::wstring& text)
{
    int len = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, len, len);
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg)
    {
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_BTN_CLOSE)
            {
                shared->exitFlag = 1;
                SetEvent(hExitEvent);
                PostQuitMessage(0);
            }
            break;

        case WM_DESTROY:
            shared->exitFlag = 1;
            SetEvent(hExitEvent);
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int cmdShow)
{
    // GUI CLASS
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"ServerWindowClass";
    RegisterClassW(&wc);

    // WINDOW
    HWND hwnd = CreateWindowW(
        L"ServerWindowClass",
        L"Global Server",      
        WS_OVERLAPPEDWINDOW,
        200, 200, 600, 550,
        NULL, NULL, hInst, NULL
    );

    hEdit = CreateWindowW(
        L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE |
        ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
        10, 10, 560, 450,
        hwnd, (HMENU)IDC_EDIT_LOG, hInst, NULL
    );

    CreateWindowW(
        L"BUTTON", L"Close server", WS_CHILD | WS_VISIBLE,
        10, 470, 150, 35,
        hwnd, (HMENU)IDC_BTN_CLOSE, hInst, NULL);

    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    // FILEMAPPING
    hMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
        PAGE_READWRITE, 0, sizeof(SharedData), MAPPING_NAME);

    shared = (SharedData*)MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    ZeroMemory(shared, sizeof(SharedData));

    hEvent = CreateEventW(NULL, FALSE, FALSE, EVENT_NAME);
    hExitEvent = CreateEventW(NULL, TRUE, FALSE, EXIT_EVENT);

    // START MESSAGE
    AppendText(L"=== SERVER STARTED ===\r\n");
    AppendText(L"Host 5000\r\n\r\n");

    MSG msg;
    while (true)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) goto end;
        }

        DWORD res = WaitForSingleObject(hEvent, 100);

        if (res == WAIT_OBJECT_0)
        {
            if (shared->exitFlag == 1)
                goto end;

            std::wstringstream wss;
            wss << L"\r\n=== New message ===\r\n";

            if (shared->clientId == 1)
                wss << L"Client1:\r\n";
            else if (shared->clientId == 2)
                wss << L"Client2:\r\n";
            else
                wss << L"Client3:\r\n";

            wss << shared->message << L"\r\n";

            AppendText(wss.str());
        }
    }

end:
    UnmapViewOfFile(shared);
    CloseHandle(hMapping);
    CloseHandle(hEvent);
    CloseHandle(hExitEvent);
    return 0;
}
