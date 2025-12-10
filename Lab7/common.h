#pragma once
#include <windows.h>

// Shared memory object + events for synchronization

#define MAPPING_NAME L"Local\\LAB7_FILEMAPPING"
#define EVENT_NAME   L"Local\\LAB7_EVENT"
#define EXIT_EVENT   L"Local\\LAB7_EXIT_EVENT"

// Structure exchanged between server and clients

struct SharedData {
    wchar_t message[2048];   // UTF-16 message (text or trapezoid parameters)
    DWORD clientId;          // Which client sent the message: 1,2,3
    DWORD sequence;          // Incremented with each send
    DWORD exitFlag;          // Server sets =1 to notify all clients to close
};
