#include <windows.h>
#include <tchar.h>
#include <iostream>

using namespace std;

// Значення за замовчуванням
int WIDTH = 400;
int HEIGHT = 400;
int R = 255, G = 0, B = 0; // червоний колір

// Константний масив рисунка (8x8)
const int IMAGE_SIZE = 8;
COLORREF image[IMAGE_SIZE][IMAGE_SIZE];

// Прототип
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Функція для створення зображення з обраним кольором
void fillImage(COLORREF baseColor) {
    for (int i = 0; i < IMAGE_SIZE; i++) {
        for (int j = 0; j < IMAGE_SIZE; j++) {
            if ((i + j) % 2 == 0)
                image[i][j] = baseColor;
            else
                image[i][j] = RGB(255 - R, 255 - G, 255 - B); // інверсія кольору
        }
    }
}

// Точка входу
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // --- Відкриття консолі для введення параметрів ---
    AllocConsole();
    FILE* in; FILE* out;
    freopen_s(&in, "CONIN$", "r", stdin);
    freopen_s(&out, "CONOUT$", "w", stdout);

    cout << "=== Lab 2: Bitmap Scaling with Input Parameters ===" << endl;

    cout << "\n\nВведіть ширину вікна (за замовчуванням 400): ";
    cin >> WIDTH;

    cout << "\n\nВведіть висоту вікна (за замовчуванням 400): ";
    cin >> HEIGHT;

    cout << "\n\nВведіть колір зображення (три значення у форматі R-G-B, від 0–255): ";
    cin >> R >> G >> B;


    COLORREF baseColor = RGB(R, G, B);
    fillImage(baseColor);

    cout << "\nПараметри встановлено: " << endl;
    cout << "Розмір: " << WIDTH << "x" << HEIGHT << endl;
    cout << "Колір: (" << R << ", " << G << ", " << B << ")" << endl;
    cout << "Закрийте консоль після завершення роботи програми." << endl;

    // --- Реєстрація класу ---
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("Lab2Window");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    // --- Створення вікна ---
    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        _T("Dushko Roman IO-32 - Lab_2"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WIDTH, HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // --- Цикл повідомлень ---
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    FreeConsole();
    return (int)msg.wParam;
}

// Основна функція обробки подій
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int winWidth = WIDTH, winHeight = HEIGHT;

    switch (msg)
    {
    case WM_SIZE:
        winWidth = LOWORD(lParam);
        winHeight = HIWORD(lParam);
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        int cellWidth = winWidth / IMAGE_SIZE;
        int cellHeight = winHeight / IMAGE_SIZE;

        for (int i = 0; i < IMAGE_SIZE; i++) {
            for (int j = 0; j < IMAGE_SIZE; j++) {
                HBRUSH brush = CreateSolidBrush(image[i][j]);
                RECT r = { j * cellWidth, i * cellHeight, (j + 1) * cellWidth, (i + 1) * cellHeight };
                FillRect(hdc, &r, brush);
                DeleteObject(brush);
            }
        }

        EndPaint(hwnd, &ps);
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
