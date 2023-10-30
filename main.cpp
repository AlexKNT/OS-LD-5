#undef UNICODE // Pārslēgt projektu ASCII režīmā
#include <windows.h>
#include <stdio.h>
#include "resource.h"
#include "TBuffer.h"
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

// Ziņojumu apstrādātājs
BOOL CALLBACK MainWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    switch (Msg) { // pārbaudīt ziņojuma identifikatoru
    case WM_INITDIALOG: // ja tas ir inicializācijas paziņojums
        // izpildīt inicializācijas darbības
        return TRUE;
    case WM_COMMAND: // ja tas ir paziņojums no vadības elementa
        switch (LOWORD(wParam)) { // pārbaudīt vadības elementa ID
        case IDOK: // ja bija uzspiesta OK poga
            DestroyWindow(hWnd); // aizvērt logu
            return TRUE;
        }
        return FALSE;
    case WM_DESTROY: // ja tas ir pabeigšanas pieprasījums
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
Process service thread
At the start of the thread creation function, save the position number of the buffer, which will be specified as a parameter during the creation of the thread:
DWORD WINAPI ProcessThread(void* APos)
{
int pos = (int)APos;
return 0;
}

Read the name of the file from the buffer and use it launch a process with CreateProcess
If CreateProcess failed then change the state of the process in the buffer to psError and end the thread.
Else if the process started then change the state of the process in the buffer to psRunning and write the process name and identifier to the buffer.
In the thread loop, check whether the process is finished, if true - end the loop if the process. If false then determine the user time and kernel time consumed by the process and add them to the buffer
After loop ends, change the state of the process in the buffer to psTerminated and close its handle
*/

/*
WM_UPDATELIST message

*/