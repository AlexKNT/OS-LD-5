// TBuffer.cpp

#include "TBuffer.h"
#include <Windows.h>

#define WM_UPDATELIST (WM_USER + 1)

TBuffer::TBuffer(HWND AWnd) {
    Wnd = AWnd;
    InitializeCriticalSection(&cs);
}

TBuffer::~TBuffer() {
    DeleteCriticalSection(&cs);
    // Cleanup any other resources if needed
}

int TBuffer::Count() {
    EnterCriticalSection(&cs);
    int count = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (Buf[i].State != psEmpty) {
            count++;
        }
    }
    LeaveCriticalSection(&cs);
    return count;
}

int TBuffer::AddProcess(char* AName) {
    EnterCriticalSection(&cs);
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (Buf[i].State == psEmpty) {
            strncpy_s(Buf[i].Name, AName, sizeof(Buf[i].Name));
            Buf[i].State = psNew;
            LeaveCriticalSection(&cs);

            // Notify the main window about the change
            PostMessage(Wnd, WM_UPDATELIST, i, 0);
            return i;
        }
    }

    LeaveCriticalSection(&cs);
    return -1; // Buffer is full
}

void TBuffer::Get(int Id, TProcessInfo& Pi) {
    EnterCriticalSection(&cs);
    if (Id >= 0 && Id < BUFFER_SIZE) {
        Pi = Buf[Id];
    }
    LeaveCriticalSection(&cs);
}

void TBuffer::Set(int Id, const TProcessInfo Pi) {
    EnterCriticalSection(&cs);
    if (Id >= 0 && Id < BUFFER_SIZE) {
        Buf[Id] = Pi;
    }
    LeaveCriticalSection(&cs);

    // Notify the main window about the change
    PostMessage(Wnd, WM_UPDATELIST, Id, 0);
}

HANDLE TBuffer::GetThreadHandle(int Id) {
    EnterCriticalSection(&cs);
    HANDLE hThread = Buf[Id].ThreadHandle;
    LeaveCriticalSection(&cs);
    return hThread;
}

void TBuffer::SetThreadHandle(int Id, HANDLE hThread) {
    EnterCriticalSection(&cs);
    Buf[Id].ThreadHandle = hThread;
    LeaveCriticalSection(&cs);
    // Notify the main window about the change (optional, depending on your needs)
    PostMessage(Wnd, WM_UPDATELIST, Id, 0);
}
