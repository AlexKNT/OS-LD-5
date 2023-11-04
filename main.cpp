#undef UNICODE // Pārslēgt projektu ASCII režīmā
#include <windows.h>
#include <stdio.h>
#include "resource.h"
#include "TBuffer.h"
#include <processthreadsapi.h>
#include <commctrl.h>
#pragma warning(disable : 4996)

TBuffer* Buffer;

// Ziņojumu apstrādātāja prototips
BOOL CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);

// Galvenā funkcija
// ja galvenā loga šablonam ir piešķirts resursa identifikators IDD_MAINDIALOG
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    DialogBox(
        hInstance,
        MAKEINTRESOURCE(IDD_MAINDIALOG),
        NULL,
        DLGPROC(MainWndProc)
    );
    return 0;
}

DWORD FileTimeToMilliseconds(FILETIME ft) {
    ULARGE_INTEGER i;
    i.LowPart = ft.dwLowDateTime;
    i.HighPart = ft.dwHighDateTime;
    return i.QuadPart / 10000;
}


DWORD WINAPI ProcessThread(void* APos)
{
    int pos = (int)APos;
    TProcessInfo processInfo;
    Buffer->Get(pos, processInfo);

    // Read the name of the file to be executed
    char fileName[MAX_PATH];
    strncpy(fileName, processInfo.Name, sizeof(fileName));

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcess(
        NULL,
        fileName, // Command line
        NULL,     // Process handle not inheritable
        NULL,     // Thread handle not inheritable
        FALSE,    // Set handle inheritance to FALSE
        0,        // No creation flags
        NULL,     // Use parent's environment block
        NULL,     // Use parent's starting directory
        &si,      // Pointer to STARTUPINFO structure
        &pi))     // Pointer to PROCESS_INFORMATION structure
    {
        // Process started successfully
        processInfo.State = psRunning;
        processInfo.PID = pi.dwProcessId;

        // Update the process info in the buffer
        Buffer->Set(pos, processInfo);

        // Monitor the process
        while (true)
        {
            // Check if the process has finished
            DWORD exitCode;
            if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
            {
                break; // Process has finished
            }

            FILETIME creationTime, exitTime, kernelTime, userTime;
            if (GetProcessTimes(pi.hProcess, &creationTime, &exitTime, &kernelTime, &userTime))
            {
                // Calculate user and kernel time in milliseconds
                DWORD userMilliseconds = FileTimeToMilliseconds(userTime);
                DWORD kernelMilliseconds = FileTimeToMilliseconds(kernelTime);

                // Calculate the time interval
                DWORD currentTime = GetTickCount();
                DWORD elapsedTime = currentTime - processInfo.UserTime; // Assuming you store the previous time

                // Calculate the percentage of time spent
                float userUsage = 100.0f * static_cast<float>(userMilliseconds - processInfo.UserTime) / elapsedTime;
                float kernelUsage = 100.0f * static_cast<float>(kernelMilliseconds - processInfo.KernelTime) / elapsedTime;

                // Update processInfo with the new time data
                processInfo.UserTime = userMilliseconds;
                processInfo.KernelTime = kernelMilliseconds;

                // Update the process info in the buffer
                Buffer->Set(pos, processInfo);
            }

            // Sleep for some time before checking again
            Sleep(1000); // Sleep for 1 second (adjust as needed)
        }

        // Process has terminated
        processInfo.State = psTerminated;

        // Update the process info in the buffer
        Buffer->Set(pos, processInfo);

        // Close the process handle
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        // CreateProcess failed
        processInfo.State = psError;

        // Update the process info in the buffer
        Buffer->Set(pos, processInfo);
    }

    return 0;
}


// Ziņojumu apstrādātājs
BOOL CALLBACK MainWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    switch (Msg) { // pārbaudīt ziņojuma identifikatoru

    case WM_INITDIALOG: // ja tas ir inicializācijas paziņojums
        Buffer = new TBuffer(hWnd);

        // Get the handle to the list control
        HWND hList = GetDlgItem(hWnd, IDC_LIST);

        // Set the extended style for full row select
        SendMessage(hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

        // Add columns to the list control
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;

       /*
           // Example:
        LVITEM lvi;
        lvi.mask = LVIF_TEXT;
        lvi.iItem = 0; // Row index
        lvi.iSubItem = 0; // Column index
        lvi.pszText = "Sample Process";
        ListView_InsertItem(hList, &lvi);
       */

        return TRUE;

    case WM_COMMAND: // ja tas ir paziņojums no vadības elementa
        switch (LOWORD(wParam)) { // pārbaudīt vadības elementa ID
        case IDOK: // ja bija uzspiesta OK poga
            DestroyWindow(hWnd); // aizvērt logu
            return TRUE;
        }
        return FALSE;

    case WM_DESTROY: // ja tas ir pabeigšanas pieprasījums
        delete Buffer;
        PostQuitMessage(0); // pabeigt programmu
        return TRUE;
    }
    return FALSE;

}

/*
And resource:
Dialog IDD_MAINDIALOG, Button IDC_BROWSEBTN, Button IDC_STARTBTN, Edit control IDC_COMMANDLINE, List control IDC_LIST, Button IDC_KILLBTN, Button IDC_DELETEBTN, Button IDCCANCEL
TBuffer.h contains this code:
#pragma once
#include <wtypes.h>

enum TProcessState { psEmpty, psNew, psRunning, psTerminated, psError };

struct TProcessInfo {
    TProcessState State;
    char Name[100];
    HANDLE Handle;
    int PID;
    int UserTime, KernelTime;
};

constexpr int BUFFER_SIZE = 100;

class TBuffer {
private:
    HWND Wnd; // loga turis, kuram ir jānosūta paziņojumi
    CRITICAL_SECTION cs; // kritiskas sekcijas mainīgais
    TProcessInfo Buf[BUFFER_SIZE]; // kolekcija ar informāciju par procesiem
public:
    int Count(); // atgriež aizņemto pozīciju daudzumu
    int AddProcess(char* AName); // atrod brīvo pozīciju un ieraksta turp procesa vārdu
    void Get(int Id, TProcessInfo& Pi); // izlasa informāciju par procesu no pozīcijas Id
    void Set(int Id, const TProcessInfo Pi); // ieraksta informāciju par procesu uz pozīciju Id
    TBuffer(HWND AWnd); // izveidošanas laikā ir jānorāda loga turis
    ~TBuffer();
};

TBuffer.cpp contains this code:
#include "TBuffer.h"

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
            strncpy(Buf[i].Name, AName, sizeof(Buf[i].Name));
            Buf[i].State = psNew;
            LeaveCriticalSection(&cs);

            // Notify the main window about the change
            PostMessage(Wnd, WM_USER + 1, i, 0);
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
    PostMessage(Wnd, WM_USER + 1, Id, 0);
}
*/

/*
Buffer
Every public method must be protected with a critical section
Each method that changes process information must send a message to the window and specify the buffer position number that contains the changed information
In WM_INITDIALOG, create a buffer object and pass hWnd to it.
In WM_DESTROY, destroy the buffer object
*/

/*
IDC_LIST initialization
In WM_INITDIALOG, initialize IDC_LIST the following way:
Set the extended style LVS_EX_FULLROWSELECT for the list so that only whole rows are selected in the list. For this, use LVM_SETEXTENDEDLISTVIEWSTYLE
Add columns to the list, specifying headings and widths. For this, use LVM_INSERTCOLUMN
*/

/*
Process service thread function
At the start of the thread creation function, save the position number of the buffer, which will be specified as a parameter during the creation of the thread:
DWORD WINAPI ProcessThread(void* APos)
{
int pos = (int)APos;
return 0;
}

Read the name of the file from the buffer and use it launch a process with CreateProcess
If CreateProcess failed then change the state of the process in the buffer to psError and end the thread.
Else if the process started then change the state of the process in the buffer to psRunning and write the process name and identifier to the buffer.
In the thread loop, check whether the process is finished, if true - end the loop. If false then determine the user time and kernel time consumed by the process and add them to the buffer
After loop ends, change the state of the process in the buffer to psTerminated and close its handle
*/

/*
WM_UPDATELIST message
Use LVM_FINDITEM message to find an element in IDC_LIST whose parameter is equal to the changed buffer position number. The changed buffer position number must be read from the wParam parameter of the message
If the element is found then read process information from the changed buffer position and write it to the corresponding list element position using LVM_SETITEM messages
*/

/*
IDC_STARTBTN case
Find the next empty position in the buffer and write there the name of the process to be launched, read from IDC_COMMANDLINE:
int pos = Buffer.AddProcess(command_line);
Add the new element to the IDC_LIST via the LVM_INSERTITEM message, specifying the buffer position number as the element parameter.
Call ProcessThread, pass to it the buffer position number
Close the handle:
CloseHandle(CreateThread(NULL, 0, ProcessThread, (LPVOID)pos, 0, NULL));
*/

/*
IDC_DELETEBTN case
Use LVM_GETSELECTIONMARK message to determine the position number of the selected list element
Use LVM_GETITEM message to read the parameter of the selected list element that will denote the the buffer position number
Delete the information about the process from the buffer
If state of the process is not equal to psRunning then delete the element from IDC_LIST using LVM_DELETEITEM and denote the buffer position number with psEmpty.
*/

/*
IDC_KILLBTN
Use LVM_GETSELECTIONMARK to determine the position number of the selected list element 
Use LVM_GETITEM to read the parameter of the selected list element that denotes the buffer position number
Read process information from the buffer
If TProcessState equals psRunning then terminate the process using TerminateProcess.
*/