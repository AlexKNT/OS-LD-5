#undef UNICODE // Pārslēgt projektu ASCII režīmā
#include <windows.h>
#include <stdio.h>
#include "resource.h"
#include "TBuffer.h"
#include <processthreadsapi.h>
#include <commctrl.h>
#pragma warning(disable : 4996)

#define WM_UPDATELIST (WM_USER + 1)

enum column { eNAME, eSTATE, eHANDLE, ePID, eUTIME, eKTIME };

TBuffer* Buffer;
HWND hList;

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
    strncpy_s(fileName, processInfo.Name, sizeof(fileName));

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char strbuf[100];

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

        ListView_SetItemText(hList, pos, eSTATE, const_cast<LPSTR>("Running"));

        memset(strbuf, 0, sizeof(strbuf));
        snprintf(strbuf, sizeof(strbuf), "%lu", pi.dwProcessId);
        ListView_SetItemText(hList, pos, ePID, strbuf);

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

                memset(strbuf, 0, sizeof(strbuf));
                snprintf(strbuf, sizeof(strbuf), "%f", userUsage);

                ListView_SetItemText(hList, pos, eUTIME, const_cast<LPSTR>(strbuf));

                memset(strbuf, 0, sizeof(strbuf));
                snprintf(strbuf, sizeof(strbuf), "%f", kernelUsage);
                ListView_SetItemText(hList, pos, eKTIME, const_cast<LPSTR>(strbuf));
            }

            // Sleep for some time before checking again
            Sleep(1000); // Sleep for 1 second (adjust as needed)
        }

        // Process has terminated
        processInfo.State = psTerminated;

        // Update the process info in the buffer
        Buffer->Set(pos, processInfo);

        ListView_SetItemText(hList, pos, eSTATE, const_cast<LPSTR>("Terminated"));

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

        ListView_SetItemText(hList, pos, eSTATE, const_cast<LPSTR>("Error"));
    }

    return 0;
}

bool BrowseForFileName(HWND hWnd, char* fileName) {
    OPENFILENAME ofn = { 0 };
    char szFileName[MAX_PATH] = { 0 };

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = "Executable Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = "exe";

    if (GetOpenFileNameA(&ofn)) {
        // Copy the selected file name to the output parameter 'fileName'
        strcpy(fileName, szFileName);
        return true;
    }

    return false;
}

// Ziņojumu apstrādātājs
BOOL CALLBACK MainWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    
    switch (Msg) { // pārbaudīt ziņojuma identifikatoru

    case WM_INITDIALOG: // ja tas ir inicializācijas paziņojums
        Buffer = new TBuffer(hWnd);

        // Get the handle to the list control
        hList = GetDlgItem(hWnd, IDC_LIST);

        // Set the extended style for full row select
        SendMessage(hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

        // Add columns to the list control
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;

        lvc.pszText = const_cast<LPSTR>("Name");
        lvc.cx = 150;
        SendMessage(hList, LVM_INSERTCOLUMN, eNAME, (LPARAM)&lvc);

        lvc.pszText = const_cast <LPSTR>("State");
        lvc.cx = 150;
        SendMessage(hList, LVM_INSERTCOLUMN, eSTATE, (LPARAM)&lvc);

        lvc.pszText = const_cast <LPSTR>("Handle");
        lvc.cx = 150;
        SendMessage(hList, LVM_INSERTCOLUMN, eHANDLE, (LPARAM)&lvc);

        lvc.pszText = const_cast <LPSTR>("PID");
        lvc.cx = 150;
        SendMessage(hList, LVM_INSERTCOLUMN, ePID, (LPARAM)&lvc);

        lvc.pszText = const_cast <LPSTR>("Time in User Mode");
        lvc.cx = 150;
        SendMessage(hList, LVM_INSERTCOLUMN, eUTIME, (LPARAM)&lvc);

        lvc.pszText = const_cast <LPSTR>("Time in Kernel Mode");
        lvc.cx = 150;
        SendMessage(hList, LVM_INSERTCOLUMN, eKTIME, (LPARAM)&lvc);

        return TRUE;

    case WM_COMMAND: // ja tas ir paziņojums no vadības elementa
        switch (LOWORD(wParam)) { // pārbaudīt vadības elementa ID
        case IDCANCEL: // ja bija uzspiesta OK poga
            DestroyWindow(hWnd); // aizvērt logu
            return TRUE;

        case IDC_BROWSEBTN:
            char fileName[MAX_PATH];
            if (BrowseForFileName(hWnd, fileName)) {
                // Update the command line field with the selected file name
                SetDlgItemTextA(hWnd, IDC_COMMANDLINE, fileName);
                return TRUE;
            }

        case IDC_STARTBTN:
            char command_line[MAX_PATH];
            HWND hEdit = GetDlgItem(hWnd, IDC_COMMANDLINE);
            GetWindowText(hEdit, command_line, sizeof(command_line));

            // Check if the command_line is not empty
            if (strlen(command_line) > 0) {
                // Find the next empty position in the buffer and write the name of the process
                int pos = Buffer->AddProcess(command_line);

                if (pos != -1) {
                    // Add the new element to the IDC_LIST
                    HWND hList = GetDlgItem(hWnd, IDC_LIST);
                    LVITEM lvi;
                    lvi.mask = LVIF_TEXT | LVIF_PARAM;
                    lvi.iItem = pos; // Row index
                    lvi.iSubItem = eNAME; // Column index
                    lvi.pszText = command_line;
                    lvi.lParam = pos; // Use lParam to store the buffer position number

                    int newItemIndex = ListView_InsertItem(hList, &lvi);

                    ListView_SetItemText(hList, newItemIndex, eSTATE, const_cast<LPSTR>("Just launched"));
                    ListView_SetItemText(hList, newItemIndex, eHANDLE, const_cast<LPSTR>("0"));
                    ListView_SetItemText(hList, newItemIndex, ePID, const_cast<LPSTR>("0"));
                    ListView_SetItemText(hList, newItemIndex, eUTIME, const_cast<LPSTR>("0"));
                    ListView_SetItemText(hList, newItemIndex, eKTIME, const_cast<LPSTR>("0"));

                    // Start a new thread to monitor the process
                    CloseHandle(CreateThread(NULL, 0, ProcessThread, (LPVOID)pos, 0, NULL));
                }
            }

            return TRUE;
        }

        case IDC_DELETEBTN:
        {
            // Get the handle to the list control
            HWND hList = GetDlgItem(hWnd, IDC_LIST);

            // Use LVM_GETSELECTIONMARK to get the selected item index
            int selectedItemIndex = ListView_GetSelectionMark(hList);

            // Use LVM_GETITEM to get the buffer position number (lParam) of the selected item
            LVITEM lvi;
            lvi.mask = LVIF_PARAM;
            lvi.iItem = selectedItemIndex;
            lvi.iSubItem = 0;
            if (ListView_GetItem(hList, &lvi))
            {
                int bufferPosition = static_cast<int>(lvi.lParam);

                // Ensure bufferPosition is valid
                if (bufferPosition >= 0 && bufferPosition < BUFFER_SIZE)
                {
                    // Get the process state
                    TProcessInfo processInfo;
                    Buffer->Get(bufferPosition, processInfo);

                    if (processInfo.State != psRunning)
                    {
                        // Delete the item from the list control using LVM_DELETEITEM
                        ListView_DeleteItem(hList, selectedItemIndex);

                        // Update the buffer position to psEmpty
                        processInfo.State = psEmpty;
                        Buffer->Set(bufferPosition, processInfo);
                    }
                    else
                    {
                        // Handle the case where the process is running (you can show an error message or take appropriate action)
                    }
                }
            }

            return TRUE;
        }

        case IDC_KILLBTN:
        {
            // Get the handle to the list control
            HWND hList = GetDlgItem(hWnd, IDC_LIST);

            // Use LVM_GETSELECTIONMARK to get the selected item index
            int selectedItemIndex = ListView_GetSelectionMark(hList);

            // Use LVM_GETITEM to get the buffer position number (lParam) of the selected item
            LVITEM lvi;
            lvi.mask = LVIF_PARAM;
            lvi.iItem = selectedItemIndex;
            lvi.iSubItem = 0;
            if (ListView_GetItem(hList, &lvi))
            {
                int bufferPosition = static_cast<int>(lvi.lParam);

                // Ensure bufferPosition is valid
                if (bufferPosition >= 0 && bufferPosition < BUFFER_SIZE)
                {
                    // Get the process state
                    TProcessInfo processInfo;
                    Buffer->Get(bufferPosition, processInfo);

                    if (processInfo.State == psRunning)
                    {
                        // Terminate the process using TerminateProcess
                        if (processInfo.Handle != NULL)
                        {
                            if (TerminateProcess(processInfo.Handle, 0))
                            {
                                // The process has been terminated successfully
                                processInfo.State = psTerminated;
                                //
                                // Update the process info in the buffer
                                Buffer->Set(bufferPosition, processInfo);
                            }
                            else
                            {
                                // Handle the case where terminating the process failed
                                // You can show an error message or take appropriate action
                                processInfo.State = psError;
                                Buffer->Set(bufferPosition, processInfo);
                            }
                        }
                    }
                    else
                    {
                        // Handle the case where the process is not running (e.g., already terminated or in an error state)
                        // You can show an error message or take appropriate action
                    }
                }
            }

            return TRUE;
        }

        return FALSE;

    case WM_UPDATELIST:
    {
        // Retrieve the buffer position number from the wParam parameter
        int bufferPosition = static_cast<int>(wParam);

        // Get the handle to the list control
        HWND hList = GetDlgItem(hWnd, IDC_LIST);

        // Use LVM_FINDITEM message to find the list item with lParam equal to the buffer position number
        LVFINDINFO lvfi;
        lvfi.flags = LVFI_PARAM;
        lvfi.lParam = bufferPosition;
        int listItemIndex = ListView_FindItem(hList, -1, &lvfi);

        if (listItemIndex != -1)
        {
            // Update the corresponding list item with process information
            TProcessInfo processInfo;
            Buffer->Get(bufferPosition, processInfo);

            LVITEM lvi;
            lvi.mask = LVIF_TEXT;
            lvi.iItem = listItemIndex;
            lvi.iSubItem = 0; // Column index
            lvi.pszText = processInfo.Name;
            ListView_SetItem(hList, &lvi);

            // You can update other columns of the list item as needed

            // Redraw the list item
            ListView_RedrawItems(hList, listItemIndex, listItemIndex);
        }

        return TRUE;
    }


    case WM_DESTROY: // ja tas ir pabeigšanas pieprasījums
        delete Buffer;
        PostQuitMessage(0); // pabeigt programmu
        return TRUE;
    }
    return FALSE;

}

/*
And resource.h contains:
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
            strncpy(Buf[i].Name, AName, sizeof(Buf[i].Name));
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