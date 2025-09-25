#include <windows.h>
#include <string>
#include <sstream>

// Default window size
int gWidth  = 500;
int gHeight = 200;

// Global variables
HINSTANCE hInst;
LPCWSTR szWindowClass = L"DushkoRoman";
LPCWSTR szTitle = L"Dushko Roman IO-32";

// Forward declarations
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    LPWSTR    lpCmdLine,
                    int       nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);

    // ----- читаємо аргументи -----
    if (lpCmdLine && wcslen(lpCmdLine) > 0) {
        std::wstringstream ss(lpCmdLine);
        int w, h;
        if (ss >> w >> h) {
            gWidth  = w;
            gHeight = h;
        }
    }

    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize        = sizeof(WNDCLASSEXW);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor       = LoadCursor(nullptr, IDC_HELP);
    wcex.hbrBackground = CreateSolidBrush(RGB(211, 211, 211));
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;

    HWND hWnd = CreateWindowW(
        szWindowClass, szTitle,
        WS_OVERLAPPEDWINDOW,
        0, 0, gWidth, gHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return FALSE;

    // кнопка Exit (піднята вище)
    CreateWindowW(
        L"BUTTON", L"Exit",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        gWidth/2 - 40, gHeight - 60, 50, 30,
        hWnd, (HMENU)1, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE)
            return 0;
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = mmi->ptMaxTrackSize.x = gWidth;
        mmi->ptMinTrackSize.y = mmi->ptMaxTrackSize.y = gHeight;
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            DestroyWindow(hWnd);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);

        RECT rect;
        GetClientRect(hWnd, &rect);

        std::wstring text =
            L"Font height: " + std::to_wstring(tm.tmHeight) + L"\n" +
            L"Line spacing: " + std::to_wstring(tm.tmHeight + tm.tmExternalLeading) + L"\n" +
            L"Window size: " + std::to_wstring(gWidth) + L"x" + std::to_wstring(gHeight);

        DrawTextW(hdc, text.c_str(), (int)text.length(), &rect, DT_CENTER);

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
