//console_pipe.h
// the console_pipe.cpp declarations .....................................................

//C style use
HRESULT RunProcess(HWND hWnd           //Window to 'SendMessage' to
    , WORD idPost        //ID for message with the data pointer in wparam
    , WORD idEnd         //ID for message notifying the process has ended
    , LPCTSTR psFileExe  //Path.File of process to run
    , LPCTSTR psCmdLine = NULL    //command line
);

//C++
extern HANDLE hGlobalThread;
extern HANDLE hChildProcess;


class RunPipeConsoleProcess
{
    HANDLE hProcess;

    WORD idPost;
    WORD idEnd;
    CString strExeFile;
    CString strArguments;

public:
    void SetMessageIDs(WORD inPost, WORD inEnd) { idPost = inPost; idEnd = inEnd; }
    void SetExecutable(LPCTSTR psExe) { strExeFile = psExe; }
    void SetArguments(LPCTSTR psArg) { strArguments = psArg; }

    bool StartThread(HWND hPostTo) { hProcess = hChildProcess; return RunProcess(hPostTo, idPost, idEnd, strExeFile, strExeFile + strArguments) == S_OK; }
    bool StopThread()
    {
        if (hGlobalThread && hProcess)
            TerminateProcess(hProcess, -1);
        //          ::PostThreadMessage( (DWORD)hGlobalThread, ID_CONSOLE_THREAD_STOP, 0, 0 );
        return true;
    }
};

//console_pipe.cpp
//#include "stdafx.h"
#include <windows.h> 
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>

#ifndef DWORD
typedef unsigned long DWORD;
#endif
#define CONPIPE_BUF_SIZE 1024

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/*++

   Copyright (c) 1998  Microsoft Corporation

   Module Name:

      Redirect.c

   Description:
       This sample illustrates how to spawn a child console based
       application with redirected standard handles.

       The following import libraries are required:
       user32.lib

   Dave McPherson (davemm)   11-March-98

   Modified 6-30-12 for use with MFC, Dan Bloomquist
   RunProcess(...)
     Creates a process running in its own thread. The process
     is expected to use STDOUT to notify the condition of the process.

     This call will return after starting the process with
     the handle of the process. All you should need to do is
     check for NULL, this means the process failed to start.

     There will be no input to the process, it is expected
     to work on its own after starting. A worker thread will
     gather the STDOUT a line at a time and send it back to
     the window specified.

     TODO: The comments below reflect sending data to the
     process input pipe. Here we are only interested in the
     output of the process.
--*/

#pragma comment(lib, "User32.lib")
void DisplayError(TCHAR* pszAPI);
void ReadAndHandleOutput(HANDLE hPipeRead);
void PrepAndLaunchRedirectedChild(HANDLE hChildStdOut,
    HANDLE hChildStdIn,
    HANDLE hChildStdErr,
    LPCTSTR psPathFile,
    LPCTSTR psCommandLine);

DWORD WINAPI GetAndSendInputThread(LPVOID lpvThreadParam);

HANDLE hChildProcess = NULL;
HANDLE hStdIn = NULL; // Handle to parents std input.
BOOL bRunThread = TRUE;
HWND hPostMessage = NULL;
HANDLE hOutputRead = NULL;
HANDLE hInputRead = NULL;
HANDLE hInputWrite = NULL;
WORD idGlbPost;
WORD idGlbEnd;
HANDLE hGlobalThread;

HRESULT RunProcess(HWND hWnd           //Window to 'SendMessage' to
    , WORD idPost        //ID for message with the data pointer in wparam
    , WORD idEnd         //ID for message notifying the process has ended
    , LPCTSTR psFileExe  //Path.File of process to run
    , LPCTSTR psCmdLine
)
{
    hPostMessage = hWnd;
    HANDLE hOutputReadTmp;
    HANDLE hOutputWrite;
    HANDLE hInputWriteTmp;
    HANDLE hErrorWrite;
    DWORD ThreadId;
    SECURITY_ATTRIBUTES sa;

    idGlbPost = idPost;
    idGlbEnd = idEnd;

    // Set up the security attributes struct.
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;


    // Create the child output pipe.
    if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0))
        DisplayError(_T("CreatePipe"));


    // Create a duplicate of the output write handle for the std error
    // write handle. This is necessary in case the child application
    // closes one of its std output handles.
    if (!DuplicateHandle(GetCurrentProcess(), hOutputWrite,
        GetCurrentProcess(), &hErrorWrite, 0,
        TRUE, DUPLICATE_SAME_ACCESS))
        DisplayError(_T("DuplicateHandle"));


    // Create the child input pipe.
    if (!CreatePipe(&hInputRead, &hInputWriteTmp, &sa, 0))
        DisplayError(_T("CreatePipe"));

    // Create new output read handle and the input write handles. Set
    // the Properties to FALSE. Otherwise, the child inherits the
    // properties and, as a result, non-closeable handles to the pipes
    // are created.
    if (!DuplicateHandle(GetCurrentProcess(), hOutputReadTmp,
        GetCurrentProcess(),
        &hOutputRead, // Address of new handle.
        0, FALSE, // Make it uninheritable.
        DUPLICATE_SAME_ACCESS))
        DisplayError(_T("DupliateHandle"));

    if (!DuplicateHandle(GetCurrentProcess(), hInputWriteTmp,
        GetCurrentProcess(),
        &hInputWrite, // Address of new handle.
        0, FALSE, // Make it uninheritable.
        DUPLICATE_SAME_ACCESS))
        DisplayError(_T("DupliateHandle"));


    // Close inheritable copies of the handles you do not want to be
    // inherited.
    if (!CloseHandle(hOutputReadTmp)) DisplayError(_T("CloseHandle hOutputReadTmp"));
    if (!CloseHandle(hInputWriteTmp)) DisplayError(_T("CloseHandle hInputWriteTmp"));


    // Get std input handle so you can close it and force the ReadFile to
    // fail when you want the input thread to exit.
    if ((hStdIn = GetStdHandle(STD_INPUT_HANDLE)) ==
        INVALID_HANDLE_VALUE)
        DisplayError(_T("GetStdHandle"));

    PrepAndLaunchRedirectedChild(hOutputWrite, hInputRead, hErrorWrite, psFileExe, psCmdLine);

    // Close pipe handles (do not continue to modify the parent).
    // You need to make sure that no handles to the write end of the
    // output pipe are maintained in this process or else the pipe will
    // not close when the child process exits and the ReadFile will hang.
    if (!CloseHandle(hOutputWrite)) DisplayError(_T("CloseHandle hOutputWrite"));
    if (!CloseHandle(hInputRead)) DisplayError(_T("CloseHandle hInputRead"));
    if (!CloseHandle(hErrorWrite)) DisplayError(_T("CloseHandle hErrorWrite"));

    // Launch the thread that gets the input and sends it to the child.
    hGlobalThread = CreateThread(NULL, 0, GetAndSendInputThread, (LPVOID)hOutputRead, 0, &ThreadId);
    if (hGlobalThread == NULL)
        return -1;

    return 0;
}

/////////////////////////////////////////////////////////////////////// 
// PrepAndLaunchRedirectedChild
// Sets up STARTUPINFO structure, and launches redirected child.
/////////////////////////////////////////////////////////////////////// 
void PrepAndLaunchRedirectedChild(HANDLE hChildStdOut,
    HANDLE hChildStdIn,
    HANDLE hChildStdErr,
    LPCTSTR psPathFile,
    LPCTSTR psCommandLine)
{
    //static int test= 0;
    //ASSERT( ! test++ );

    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    // Set up the start up info struct.
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hChildStdOut;
    si.hStdInput = hChildStdIn;
    si.hStdError = hChildStdErr;
    // Use this if you want to hide the child:
    //     si.wShowWindow = SW_HIDE;
    // Note that dwFlags must include STARTF_USESHOWWINDOW if you want to
    // use the wShowWindow flags.


    // Launch the process that you want to redirect (in this case,
    // Child.exe). Make sure Child.exe is in the same directory as
    // redirect.c launch redirect from a command line to prevent location
    // confusion.
    BOOL bSuccess = FALSE;
    bSuccess = CreateProcess(
        psPathFile,     // app console
        const_cast<LPTSTR>(psCommandLine),    //command line
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        CREATE_NO_WINDOW,   // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &si,  // STARTUPINFO pointer 
        &pi  // receives PROCESS_INFORMATION 
    );
    // If an error occurs, exit the application. 
    if (!bSuccess)
    {
        DWORD err = GetLastError();
        DisplayError(TEXT("CreateProcess"));
    }
    //if (!CreateProcess(NULL,psTestFile,NULL,NULL,TRUE,
    //                    CREATE_NEW_CONSOLE,NULL,NULL,&si,&pi))
    //    DisplayError(_T("CreateProcess");


    // Set global child process handle to cause threads to exit.
    hChildProcess = pi.hProcess;


    // Close any unnecessary handles.
    if (!CloseHandle(pi.hThread))
        DisplayError(_T("CloseHandle pi.hThread"));
}


/////////////////////////////////////////////////////////////////////// 
// ReadAndHandleOutput
// Monitors handle for input. Exits when child exits or pipe breaks.
/////////////////////////////////////////////////////////////////////// 
//#include <boost/iostreams/device/file_descriptor.hpp>
//#include <boost/iostreams/stream.hpp>
void ReadAndHandleOutput(HANDLE hPipeRead)
{
    ASSERT(false);
    int file_descriptor = _open_osfhandle((intptr_t)hPipeRead, 0);
    FILE* file = _fdopen(file_descriptor, "r");
    std::ifstream stream(file);
    std::string line;

    //loop until stream empty
    for (; std::getline(stream, line); )
    {
        SendMessage(hPostMessage, idGlbPost, (LPARAM)line.c_str(), 0);
        TRACE("line: %s\n", line.c_str());
    }
    //should be so
    ASSERT(GetLastError() == ERROR_BROKEN_PIPE);
    if (!CloseHandle(hStdIn)) DisplayError(_T("CloseHandle hStdIn"));
    if (!CloseHandle(hOutputRead)) DisplayError(_T("CloseHandle hOutputRead"));
    if (!CloseHandle(hInputWrite)) DisplayError(_T("CloseHandle hInputWrite"));
}


/////////////////////////////////////////////////////////////////////// 
// GetAndSendInputThread
// Thread procedure that monitors the console for input and sends input
// to the child process through the input pipe.
// This thread ends when the child application exits.
/////////////////////////////////////////////////////////////////////// 
DWORD WINAPI GetAndSendInputThread(LPVOID lpvThreadParam)
{
    HANDLE hPipeRead = (HANDLE)lpvThreadParam;
    int file_descriptor = _open_osfhandle((intptr_t)hPipeRead, 0);
    FILE* file = _fdopen(file_descriptor, "r");
    std::ifstream stream(file);
    std::string line;

    //loop until stream empty
    for (; std::getline(stream, line); )
    {
        SendMessage(hPostMessage, idGlbPost, (LPARAM)line.c_str(), 0);
        TRACE("line: %s\n", line.c_str());
    }
    //should be so
    return 1;

    ASSERT(GetLastError() == ERROR_BROKEN_PIPE);

    //if (!CloseHandle(hStdIn)) DisplayError(_T("CloseHandle"));
    if (!CloseHandle(hOutputRead)) DisplayError(_T("CloseHandle hOutputRead"));
    if (!CloseHandle(hInputWrite)) DisplayError(_T("CloseHandle hInputWrite"));

    SendMessage(hPostMessage, idGlbEnd, 0, 0);
    return 1;
}
/////////////////////////////////////////////////////////////////////// 
// DisplayError
// Displays the error number and corresponding message.
/////////////////////////////////////////////////////////////////////// 
void DisplayError(TCHAR* pszAPI)
{
    TRACE("DisplayError: %s\n", pszAPI);
    return;

    //  return;
    LPVOID lpvMessageBuffer;
    TCHAR szPrintBuffer[CONPIPE_BUF_SIZE];
    DWORD nCharsWritten;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpvMessageBuffer, 0, NULL);

    StringCbPrintf(szPrintBuffer, CONPIPE_BUF_SIZE,
        _T("ERROR: API    = %s.\n   error code = %d.\n   message    = %s.\n"),
        pszAPI, GetLastError(), (TCHAR*)lpvMessageBuffer);

    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), szPrintBuffer,
        lstrlen(szPrintBuffer), &nCharsWritten, NULL);

    LocalFree(lpvMessageBuffer);
    //ExitProcess(GetLastError());
}