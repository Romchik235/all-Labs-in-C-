// server.cpp (MAILSLOT SERVER)

#include <windows.h>
#include <string>
#include <sstream>

#define IDC_EDIT_LOG 2001
HWND hEdit;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void AppendText(const std::string& text) {
    int len = GetWindowTextLengthA(hEdit);
    SendMessageA(hEdit, EM_SETSEL, len, len);
    SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {

    // === GUI ===
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "MailslotServerClass";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0, "MailslotServerClass", "SERVER (MAILSLOT)",
        WS_OVERLAPPEDWINDOW, 100, 100, 520, 420,
        NULL, NULL, hInst, NULL
    );

    hEdit = CreateWindowExA(
        0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE |
        WS_VSCROLL | ES_AUTOVSCROLL | ES_READONLY,
        10, 10, 480, 350, hwnd, (HMENU)IDC_EDIT_LOG, hInst, NULL
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // === MAILSLOT ===
    HANDLE hSlot = CreateMailslotA(
        "\\\\.\\mailslot\\lab5_slot",
        0,
        MAILSLOT_WAIT_FOREVER,
        NULL
    );

    if (hSlot == INVALID_HANDLE_VALUE) {
        MessageBoxA(NULL, "Error creating mailslot!", "ERROR", MB_OK);
        return 1;
    }

    AppendText("=== SERVER STARTED ===\r\n\r\n");

    MSG msg{};
    while (true) {

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                return 0;
        }

        DWORD nextSize = 0, msgCount = 0;
        if (!GetMailslotInfo(hSlot, NULL, &nextSize, &msgCount, NULL))
            continue;

        if (nextSize == MAILSLOT_NO_MESSAGE) {
            Sleep(250);
            continue;
        }

        while (msgCount > 0) {
            std::string buffer(nextSize, '\0');
            DWORD bytesRead;

            ReadFile(hSlot, &buffer[0], nextSize, &bytesRead, NULL);

            AppendText("\r\n\r\n === New message === \r\n\r\n");
            AppendText(buffer + "\r\n");

            GetMailslotInfo(hSlot, NULL, &nextSize, &msgCount, NULL);
        }
    }

    CloseHandle(hSlot);
    return 0;
}
