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