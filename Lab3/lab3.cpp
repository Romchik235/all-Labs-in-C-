#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <sstream>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <algorithm>

static const int GLOBAL_MAX_THREADS_LIMIT = 50;
static const DWORD PAUSE_WAIT_TIMEOUT_MS = 3000; // 3 секунди очікування на паузу
static const int COLUMN_WIDTH = 12; // ширина колонки для вирівнювання у виводі

// --- Globals ---

static CRITICAL_SECTION g_cs;

static HWND g_hwnd = NULL;
static HWND g_hwndOutput = NULL;
static HANDLE g_hMutexConsole = NULL; // щоб писати в консоль потокобезпечно
static HANDLE g_hMutexDraw = NULL;    // захист для оновлення таблиці і EDIT
static HANDLE g_hPauseEvent = NULL;   // подія паузи (автоматична/ручна)

static std::atomic<int> g_activeThreads(0);
static std::atomic<int> g_maxThreads(20); // за замовчуванням 20 (0..20 якщо не введено інше)

static std::vector<HANDLE> g_threadHandles;
static std::mutex g_handlesMutex;

// таблиця: кожен стовпець = вектор рядків (string); при додаванні значення потік пише у свій стовпець
static std::vector<std::vector<std::string>> g_columns;
static std::vector<bool> g_columnsAsc; // true = ascending, false = descending
static std::mutex g_columnsMutex;

static bool g_consoleAllocated = false;

// range (може бути дробове)
static double g_rangeStart = 1.0;
static double g_rangeEnd = 10.0;

static const char* AUTHOR_LINE = "Dushko Roman IO-32";

// --- Utility functions ---
void ConsoleWriteSafe(const std::string& s) {
    if (g_hMutexConsole) WaitForSingleObject(g_hMutexConsole, INFINITE);
    std::cout << s << std::flush;
    if (g_hMutexConsole) ReleaseMutex(g_hMutexConsole);
}

static std::string now_time_str() {
    std::time_t t = std::time(nullptr);
    char buf[64];
    tm local_tm;
    localtime_s(&local_tm, &t);
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &local_tm);
    return std::string(buf);
}

static bool is_effectively_int(double v) {
    double r = std::round(v);
    return std::fabs(v - r) < 1e-9;
}

static std::string format_value(double v) {
    std::ostringstream oss;
    if (is_effectively_int(v)) oss << (long long)std::llround(v); // ціле число
    else {
        oss.setf(std::ios::fixed);
        oss << std::setprecision(2) << v; // дробове з двома знаками
    }
    return oss.str();
}

//  (вирівнювання колонок)
void rebuild_output_edit() {

    if (!g_hwndOutput) return;
    if (g_hMutexDraw) WaitForSingleObject(g_hMutexDraw, INFINITE);

    size_t cols = g_columns.size();
    size_t rows = 0;
    for (auto &c : g_columns) if (c.size() > rows) rows = c.size();

    std::ostringstream oss;
    // заголовок: індекси колонок
    for (size_t ci = 0; ci < cols; ++ci) {
        std::string h = (g_columnsAsc[ci] ? "Ask" : "Desk") + std::to_string(ci + 1);
        int hp = (COLUMN_WIDTH - (int)h.length()) / 2;
        if (hp > 0) oss << std::string(hp, ' ');
        oss << h;
        oss << std::string(COLUMN_WIDTH - hp - (int)h.length(), ' ');
    }
    oss << "\r\n";

    for (size_t r = 0; r < rows; ++r) {
        for (size_t ci = 0; ci < cols; ++ci) {
            if (r < g_columns[ci].size()) {
                std::string s = g_columns[ci][r];
                int padding = (COLUMN_WIDTH - (int)s.length()) / 2;
                if (padding > 0) oss << std::string(padding, ' ');
                oss << s;
                oss << std::string(COLUMN_WIDTH - padding - (int)s.length(), ' ');
            } else {
                oss << std::string(COLUMN_WIDTH, ' '); // порожня колонка
            }
        }
        oss << "\r\n";
    }

    std::string out = oss.str();
    // встановлюємо текст у EDIT
    SetWindowTextA(g_hwndOutput, out.c_str());

    if (g_hMutexDraw) ReleaseMutex(g_hMutexDraw);
}

void add_value_to_column(int colIndex, const std::string &value) {
    std::lock_guard<std::mutex> lk(g_columnsMutex);
    if (colIndex < 0 || (size_t)colIndex >= g_columns.size()) return;
    g_columns[colIndex].push_back(value);
    rebuild_output_edit(); // оновлення EDIT після кожного додавання
}

// --- Thread info ---
struct GuiThreadInfo {
    POINT pt;
    bool increasing; // true = ascending (right mouse); false = descending (left)
    double start;
    double end;
    double step;
    int columnIndex; // індекс колонки яку займає цей потік
};

// Reaper закриває handle після завершення
DWORD WINAPI ReaperWorker(LPVOID param) {
    HANDLE h = (HANDLE)param;
    if (!h) { ExitThread(1); return 1; }
    WaitForSingleObject(h, INFINITE);
    {
        std::lock_guard<std::mutex> lk(g_handlesMutex);
        auto it = std::find(g_threadHandles.begin(), g_threadHandles.end(), h);
        if (it != g_threadHandles.end()) g_threadHandles.erase(it);
    }
    CloseHandle(h);
    ExitThread(0);
    return 0;
}

// --- Gui worker ---
DWORD WINAPI GuiWorker(LPVOID lpParam) {
    GuiThreadInfo* ti = reinterpret_cast<GuiThreadInfo*>(lpParam);
    if (!ti) { ExitThread(1); return 1; }

    DWORD tid = GetCurrentThreadId();
    ConsoleWriteSafe("[" + now_time_str() + "] Потік START tid=" + std::to_string(tid) + (ti->increasing ? " (зрост.)\n" : " (спад.)\n"));

    InterlockedIncrement((volatile LONG*)&g_activeThreads);

    double s = ti->start;
    double e = ti->end;
    bool asc = ti->increasing;
    double step = ti->step;
    // if (!asc) std::swap(s, e); // для циклу: завжди ітеруємо від s до e; direction враховано нижче

    auto cmp_le = [](double a, double b) { return a <= b + 1e-9; };
    auto cmp_ge = [](double a, double b) { return a + 1e-9 >= b; };

    bool aborted_by_timeout = false;
    int col = ti->columnIndex;

    if (asc) {
    for (double v = s; cmp_le(v, e); v += step) {
        DWORD waitRes = WaitForSingleObject(g_hPauseEvent, PAUSE_WAIT_TIMEOUT_MS);
        if (waitRes == WAIT_TIMEOUT) {
            ConsoleWriteSafe("[" + now_time_str() + "] Потік tid=" + std::to_string(tid) + " — тайм-аут паузи, завершення.\n");
            aborted_by_timeout = true;
            break;
        }
        ConsoleWriteSafe("[" + now_time_str() + "] Потік tid=" + std::to_string(tid) + " доступ до GUI: " + format_value(v) + "\n");
        add_value_to_column(col, format_value(v));
        Sleep(120);
    } 
    } else { // descending
        double start_val = (s > e ? s : e);
        double end_val   = (s < e ? s : e);
        for (double v = start_val; cmp_ge(v, end_val); v -= step) {
            DWORD waitRes = WaitForSingleObject(g_hPauseEvent, PAUSE_WAIT_TIMEOUT_MS);
            if (waitRes == WAIT_TIMEOUT) {
                ConsoleWriteSafe("[" + now_time_str() + "] Потік tid=" + std::to_string(tid) + " — тайм-аут паузи, завершення.\n");
                aborted_by_timeout = true;
                break;
            }
            ConsoleWriteSafe("[" + now_time_str() + "] Потік tid=" + std::to_string(tid) + " доступ до GUI: " + format_value(v) + "\n");
            add_value_to_column(col, format_value(v));
            Sleep(120);
        }
    }

    if (!aborted_by_timeout) {
        ConsoleWriteSafe("[" + now_time_str() + "] Потік END tid=" + std::to_string(tid) + "\n");
    }

    InterlockedDecrement((volatile LONG*)&g_activeThreads);

    delete ti;
    ExitThread(0);
    return 0;
}

// --- spawn worker ---
// allocates a new column and launches the thread
void spawn_worker_at(HWND hwnd, int x, int y, bool asc) {
    int colIndex = -1;
    {
        std::lock_guard<std::mutex> lk(g_columnsMutex);
        // Шукаємо перший вільний стовпець
        for (size_t i = 0; i < g_columns.size(); ++i) {
            if (g_columns[i].empty()) {
                colIndex = (int)i;
                g_columnsAsc[i] = asc;
                break;
            }
        }

        // Якщо ще не досягли максимуму, створюємо новий стовпець
        if (colIndex == -1 && g_columns.size() < (size_t)g_maxThreads) {
            g_columns.emplace_back();
            g_columnsAsc.push_back(asc);
            colIndex = (int)g_columns.size() - 1;
        }

        // Якщо перевищили максимум
        if (colIndex == -1) {
            if (!g_ignore_column_full_error)
                MessageBoxA(hwnd, "Всі колонки/потоки вже створені!", "Помилка", MB_OK | MB_ICONERROR);
            return;
        }
    }

    GuiThreadInfo* ti = new GuiThreadInfo;
    ti->pt.x = x; ti->pt.y = y;
    ti->increasing = asc;
    ti->start = g_rangeStart;
    ti->end = g_rangeEnd;
    ti->step = (is_effectively_int(ti->start) && is_effectively_int(ti->end)) ? 1.0 : 0.1;
    ti->columnIndex = colIndex;

    HANDLE h = CreateThread(NULL, 0, GuiWorker, ti, 0, NULL);
    if (h) {
        std::lock_guard<std::mutex> lk(g_handlesMutex);
        g_threadHandles.push_back(h);
        HANDLE hr = CreateThread(NULL, 0, ReaperWorker, h, 0, NULL);
        if (hr) CloseHandle(hr);
    } else {
        delete ti;
        MessageBoxA(hwnd, "Не вдалося створити потік", "Помилка", MB_OK | MB_ICONERROR);
    }
}

// --- Experiment (як і раніше) ---
struct SumTaskArg {
    uint32_t* data;
    size_t start, end;
    unsigned long long result;
};
DWORD WINAPI SumWorker(LPVOID param) {
    SumTaskArg* a = (SumTaskArg*)param;
    unsigned long long sum = 0;
    for (size_t i = a->start; i < a->end; ++i) sum += a->data[i];
    a->result = sum;
    ExitThread(0);
    return 0;
}
unsigned long long filetime_to_100ns(const FILETIME& ft) {
    ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime; return u.QuadPart;
}
double get_process_cpu_seconds() {
    FILETIME c, e, k, u;
    if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) return 0.0;
    return double(filetime_to_100ns(k) + filetime_to_100ns(u)) / 1e7;
}
std::pair<long long, double> run_one_experiment(int threadCount, size_t N, uint32_t* data) {
    if (threadCount <= 0) return {0LL, 0.0};
    std::vector<SumTaskArg> args(threadCount);
    std::vector<HANDLE> threads(threadCount, NULL);
    size_t chunk = N / threadCount;
    for (int i = 0; i < threadCount; ++i) {
        size_t s = i * chunk;
        size_t e = (i == threadCount - 1 ? N : s + chunk);
        args[i] = { data, s, e, 0 };
    }
    double cpu_before = get_process_cpu_seconds();
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < threadCount; ++i) {
        threads[i] = CreateThread(NULL, 0, SumWorker, &args[i], 0, NULL);
        if (!threads[i]) {
            for (int j = 0; j < i; ++j) if (threads[j]) WaitForSingleObject(threads[j], INFINITE);
            for (int j = 0; j < i; ++j) if (threads[j]) CloseHandle(threads[j]);
            return {-1LL, 0.0};
        }
    }
    WaitForMultipleObjects((DWORD)threadCount, threads.data(), TRUE, INFINITE);
    auto t1 = std::chrono::high_resolution_clock::now();
    double cpu_after = get_process_cpu_seconds();
    for (int i = 0; i < threadCount; ++i) if (threads[i]) CloseHandle(threads[i]);
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    double wall_sec = double(ms) / 1000.0;
    double cpu_percent = (wall_sec > 0.0 ? ((cpu_after - cpu_before) / wall_sec) * 100.0 : 0.0);
    unsigned long long total = 0;
    for (int i = 0; i < threadCount; ++i) total += args[i].result;
    (void)total;
    return {ms, cpu_percent};
}
void run_experiment_async() {
    ConsoleWriteSafe("Експеримент: підготовка масиву (10M елементів)...\n");
    const size_t N = 10'000'000;
    uint32_t* data = (uint32_t*)malloc(sizeof(uint32_t) * N);
    if (!data) { ConsoleWriteSafe("Не вдалося виділити пам'ять\n"); return; }
    for (size_t i = 0; i < N; ++i) data[i] = i & 0xFFFF;
    std::vector<int> tests = { 2, 4, 8, 16 };
    SYSTEM_INFO si; GetSystemInfo(&si);
    ConsoleWriteSafe("Логічних процесорів: " + std::to_string(si.dwNumberOfProcessors) + "\n");
    ConsoleWriteSafe("Запуск експериментів (потоки | час ms | CPU %)\n");
    for (int t : tests) {
        if (t > si.dwNumberOfProcessors * 4) { ConsoleWriteSafe(std::to_string(t) + " пропущено\n"); continue; }
        auto res = run_one_experiment(t, N, data);
        if (res.first < 0) ConsoleWriteSafe("Експеримент " + std::to_string(t) + " потоків не вдався\n");
        else ConsoleWriteSafe("Потоків: " + std::to_string(t) + " ... " + std::to_string(res.first) + " ms, " + std::to_string((int)res.second) + " %\n");
    }
    free(data);
    ConsoleWriteSafe("Експеримент завершено.\n");
}

// --- GUI ---
#define IDC_EDIT_LIMIT 2001
#define IDC_BTN_SET 2002
#define IDC_BTN_PAUSE 2004
#define IDC_BTN_WAIT 2005
#define IDC_BTN_EXP 2006
#define IDC_BTN_EXIT 2007

// Фонова операція очікування (не блокує GUI)
void background_wait_all_and_report() {
    std::vector<HANDLE> handles;
    {
        std::lock_guard<std::mutex> lk(g_handlesMutex);
        handles = g_threadHandles;
    }
    if (handles.empty()) {
        ConsoleWriteSafe("Немає активних потоків для очікування.\n");
        return;
    }
    const DWORD total_timeout = PAUSE_WAIT_TIMEOUT_MS;
    auto tstart = std::chrono::high_resolution_clock::now();
    // Wait in batches if нужно
    size_t idx = 0;
    bool timed_out = false;
    while (idx < handles.size()) {
        size_t batch = std::min(handles.size() - idx, (size_t)MAXIMUM_WAIT_OBJECTS);
        DWORD res = WaitForMultipleObjects((DWORD)batch, &handles[idx], TRUE, total_timeout);
        if (res == WAIT_TIMEOUT) { timed_out = true; break; }
        idx += batch;
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - tstart).count() >= total_timeout) { timed_out = true; break; }
    }
    if (timed_out) ConsoleWriteSafe("Очікування дочірніх потоків — тайм-аут.\n");
    else ConsoleWriteSafe("Усі дочірні потоки завершилися успішно.\n");
}

// Обробник повідомлень
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        CreateWindowA("STATIC", "Ліміт потоків (0–20):", WS_VISIBLE | WS_CHILD, 10, 6, 140, 20, hwnd, NULL, NULL, NULL);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", std::to_string((int)g_maxThreads.load()).c_str(),
            WS_CHILD | WS_VISIBLE | ES_LEFT, 155, 4, 40, 22, hwnd, (HMENU)IDC_EDIT_LIMIT, NULL, NULL);
        CreateWindowA("BUTTON", "Застосувати", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 205, 4, 90, 22, hwnd, (HMENU)IDC_BTN_SET, NULL, NULL);
        CreateWindowA("BUTTON", "Пауза / Продовжити", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 305, 4, 140, 22, hwnd, (HMENU)IDC_BTN_PAUSE, NULL, NULL);
        CreateWindowA("BUTTON", "Дочекатися всіх", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 455, 4, 120, 22, hwnd, (HMENU)IDC_BTN_WAIT, NULL, NULL);
        CreateWindowA("BUTTON", "Експеримент", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 585, 4, 90, 22, hwnd, (HMENU)IDC_BTN_EXP, NULL, NULL);
        CreateWindowA("BUTTON", "Завершити програму", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 685, 4, 140, 22, hwnd, (HMENU)IDC_BTN_EXIT, NULL, NULL);

        g_hwndOutput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 40, 960, 600, hwnd, NULL, NULL, NULL);

        SetWindowTextA(hwnd, ("Lab3 - " + std::string(AUTHOR_LINE) + " - макс. потоків = " + std::to_string((int)g_maxThreads.load())).c_str());
        ConsoleWriteSafe("GUI створено. Натискай ліву/праву кнопку в будь-якому місці вікна, щоб створити потік.\n");
        return 0;
    }

    case WM_PARENTNOTIFY: {
        UINT msgChild = LOWORD(wParam);
        if (msgChild == WM_LBUTTONDOWN || msgChild == WM_RBUTTONDOWN) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            bool asc = (msgChild == WM_RBUTTONDOWN);
            spawn_worker_at(hwnd, x, y, asc);
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (code != BN_CLICKED) break;
        if (id == IDC_BTN_SET) {
            char buf[64] = {};
            GetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_LIMIT), buf, sizeof(buf));
            int val = 20; // дефолт
            if (strlen(buf) > 0) {
                std::istringstream iss(buf);
                if (!(iss >> val)) {
                    MessageBoxA(hwnd, "Помилка: введіть ціле число 0–20", "Помилка вводу", MB_OK | MB_ICONERROR);
                    SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_LIMIT), std::to_string((int)g_maxThreads.load()).c_str());
                    break;
                }
            }
            if (val < 0) val = 0;
            if (val > 20) val = 20;
            g_maxThreads = val;
            SetWindowTextA(hwnd, ("Lab3 - " + std::string(AUTHOR_LINE) + " - макс. потоків = " + std::to_string((int)g_maxThreads.load())).c_str());
            ConsoleWriteSafe("Максимальна кількість потоків встановлена: " + std::to_string((int)g_maxThreads.load()) + "\n");
        } else if (id == IDC_BTN_PAUSE) {
            DWORD st = WaitForSingleObject(g_hPauseEvent, 0);
            if (st == WAIT_OBJECT_0) {
                ResetEvent(g_hPauseEvent);
                ConsoleWriteSafe("Пауза встановлена.\n");
            } else {
                SetEvent(g_hPauseEvent);
                ConsoleWriteSafe("Пауза скасована. Потоки відновлено.\n");
            }
        } else if (id == IDC_BTN_WAIT) {
            ConsoleWriteSafe("Запуск фонової операції: очікування завершення потоків (тайм-аут)...\n");
            std::thread([]() { background_wait_all_and_report(); }).detach();
        } else if (id == IDC_BTN_EXP) {
            std::thread([]() { run_experiment_async(); }).detach();
        } else if (id == IDC_BTN_EXIT) {
            // підтвердження виходу (варіант B)
            int r = MessageBoxA(hwnd, "Ви дійсно бажаєте вийти?", "Підтвердження", MB_YESNO | MB_ICONQUESTION);
            if (r == IDYES) SendMessage(hwnd, WM_CLOSE, 0, 0);
        }
        break;
    }

    case WM_DESTROY: {
        // відновимо потоки (щоб вони не сиділи на WaitForSingleObject)
        SetEvent(g_hPauseEvent);

        // чекаємо потоки з таймаутом, щоб не блокувати GUI
        std::vector<HANDLE> handles;
        {
            std::lock_guard<std::mutex> lk(g_handlesMutex);
            handles = g_threadHandles;
        }
        if (!handles.empty()) {
            DWORD total_wait = PAUSE_WAIT_TIMEOUT_MS;
            DWORD start = GetTickCount();
            if (handles.size() <= MAXIMUM_WAIT_OBJECTS) {
                WaitForMultipleObjects((DWORD)handles.size(), handles.data(), TRUE, total_wait);
            } else {
                for (HANDLE h : handles) {
                    DWORD elapsed = GetTickCount() - start;
                    if (elapsed >= total_wait) break;
                    WaitForSingleObject(h, total_wait - elapsed);
                }
            }
        }
        for (HANDLE h : handles) if (h) CloseHandle(h);

        if (g_hMutexConsole) { CloseHandle(g_hMutexConsole); g_hMutexConsole = NULL; }
        if (g_hMutexDraw) { CloseHandle(g_hMutexDraw); g_hMutexDraw = NULL; }
        if (g_hPauseEvent) { CloseHandle(g_hPauseEvent); g_hPauseEvent = NULL; }

        PostQuitMessage(0);
        return 0;
    }
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- GUI Run ---
int run_gui_mode(HINSTANCE hInstance, int nCmdShow) {
    const char* CLASS_NAME = "Lab3MouseThreadsClass";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Не вдалося зареєструвати клас вікна", "Помилка", MB_OK | MB_ICONERROR);
        return 1;
    }
    HWND hwnd = CreateWindowExA(0, CLASS_NAME, ("Lab3 - " + std::string(AUTHOR_LINE)).c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 1000, 700, NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        MessageBoxA(NULL, "Не вдалося створити вікно", "Помилка", MB_OK | MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, nCmdShow);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

// --- Startup prompt ---
void prompt_startup_inputs() {
    if (!g_consoleAllocated) {
        AllocConsole();
        g_consoleAllocated = true;
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONIN$", "r", stdin);
    }
    ConsoleWriteSafe("=== Лабораторна 3: потоки за кліками миші ===\nАвтор: " + std::string(AUTHOR_LINE) + "\n");

    if (!g_hMutexConsole) g_hMutexConsole = CreateMutex(NULL, FALSE, NULL);
    if (!g_hMutexDraw) g_hMutexDraw = CreateMutex(NULL, FALSE, NULL);
    if (!g_hPauseEvent) g_hPauseEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

    // max threads (int) 0–20
    while (true) {
        ConsoleWriteSafe("Введіть максимальну кількість потоків одночасно (0–20) [Enter = 20]: ");
        std::string line; if (!std::getline(std::cin, line)) continue;
        if (line.empty()) { g_maxThreads = 20; break; }
        int val; std::istringstream iss(line);
        if (!(iss >> val)) { ConsoleWriteSafe("Помилка: введіть ціле число від 0 до 20.\n"); continue; }
        if (val < 0 || val > 20) { ConsoleWriteSafe("Помилка: число повинно бути від 0 до 20.\n"); continue; }
        g_maxThreads = val; break;
    }

    // range start (double)
    while (true) {
        ConsoleWriteSafe("Введіть початок діапазону (ціле або дробове число) [Enter = 1.0]: ");
        std::string line; if (!std::getline(std::cin, line)) continue;
        if (line.empty()) { g_rangeStart = 1.0; break; }
        double a; std::istringstream iss(line);
        if (!(iss >> a)) { ConsoleWriteSafe("Помилка: введіть число (ціле або з плаваючою комою).\n"); continue; }
        g_rangeStart = a; break;
    }

    // range end (double)
    while (true) {
        ConsoleWriteSafe("Введіть кінець діапазону (ціле або дробове число) [Enter = 10.0]: ");
        std::string line; if (!std::getline(std::cin, line)) continue;
        if (line.empty()) { g_rangeEnd = 10.0; break; }
        double b; std::istringstream iss(line);
        if (!(iss >> b)) { ConsoleWriteSafe("Помилка: введіть число (ціле або з плаваючою комою).\n"); continue; }
        g_rangeEnd = b; break;
    }

    if (g_rangeStart > g_rangeEnd) std::swap(g_rangeStart, g_rangeEnd);

    ConsoleWriteSafe("Конфігурація: максПотоків=" + std::to_string((int)g_maxThreads.load()) +
        " діапазон=[" + format_value(g_rangeStart) + "," + format_value(g_rangeEnd) + "]\n");
}

// Main
int main(int argc, char* argv[]) {
    // Ініціалізація критичної секції для додаткового локального захисту
    InitializeCriticalSection(&g_cs);

    if (argc > 1) {
        std::string mode = argv[1];
        if (mode == "experiment") {
            if (!g_consoleAllocated) { AllocConsole(); g_consoleAllocated = true; FILE* f; freopen_s(&f, "CONOUT$", "w", stdout); freopen_s(&f, "CONIN$", "r", stdin); }
            if (!g_hMutexConsole) g_hMutexConsole = CreateMutex(NULL, FALSE, NULL);
            run_experiment_async();
            return 0;
        }
    }

    prompt_startup_inputs();

    if (!g_hMutexConsole) g_hMutexConsole = CreateMutex(NULL, FALSE, NULL);
    if (!g_hMutexDraw) g_hMutexDraw = CreateMutex(NULL, FALSE, NULL);
    if (!g_hPauseEvent) g_hPauseEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

    HINSTANCE hInstance = GetModuleHandle(NULL);
    int res = run_gui_mode(hInstance, SW_SHOWDEFAULT);

    if (g_hMutexConsole) { CloseHandle(g_hMutexConsole); g_hMutexConsole = NULL; }
    if (g_hMutexDraw) { CloseHandle(g_hMutexDraw); g_hMutexDraw = NULL; }
    if (g_hPauseEvent) { CloseHandle(g_hPauseEvent); g_hPauseEvent = NULL; }
    if (g_consoleAllocated) { FreeConsole(); g_consoleAllocated = false; }

    DeleteCriticalSection(&g_cs);
    return res;
}
