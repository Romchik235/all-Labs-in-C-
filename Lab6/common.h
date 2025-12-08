#pragma once
#include <windows.h>

#define MAPPING_NAME L"Local\\LAB6_FILEMAPPING"
#define EVENT_NAME   L"Local\\LAB6_EVENT"
#define EXIT_EVENT   L"Local\\LAB6_EXIT_EVENT"

struct SharedData {
    wchar_t message[1024];   // текст від клієнта
    DWORD clientId;          // 1, 2 або 3
    DWORD sequence;          // номер повідомлення
    DWORD exitFlag;          // використовується при закритті сервера
};
