
// WSLTuxDlg.cpp : implementation file
// 
// WSL Tux is a system tray application to let you know that WSL is running by placing a Tux icon in the system tray
// 
// (c)2022 Derek Snider (DSD Software)
//
// Some code and ideas were taken from: http://www.flounder.com/


#include "pch.h"
#include "framework.h"
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include "WSLTux.h"
#include "WSLTuxDlg.h"
#include "afxdialogex.h"
#include "wslapi.h"
#include <io.h>
#include "console_pipe.h"
#include "WslApiLoader.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale>         // std::wstring_convert
#include <codecvt>        // std::codecvt_utf8
#include <winternl.h>
#pragma comment(lib,"ntdll.lib")
#include <tchar.h>
#include <psapi.h>

typedef std::codecvt_utf8<wchar_t> ccvt;
WSLInfo* _wslinfo = NULL;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static const UINT UWM_ARE_YOU_ME = ::RegisterWindowMessage(_T("UWM_ARE_YOU_ME"));

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
	ON_BN_CLICKED(IDOK, &CAboutDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CWSLTuxDlg dialog

CWSLTuxDlg::CWSLTuxDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_WSLTUX_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_hIcon_disabled = AfxGetApp()->LoadIcon(IDI_TUXDISABLED);
	m_cmdline = AfxGetApp()->m_lpCmdLine;
	_wslinfo = &this->wslinfo;

	if (m_cmdline.Find(_T("--start-minimized")) != -1)
		m_visible = false;
	else
		m_visible = true;
}

void CWSLTuxDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_WSLlistCtrl);
}

#pragma warning( push )
#pragma warning( disable : 26454 )
BEGIN_MESSAGE_MAP(CWSLTuxDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, &CWSLTuxDlg::OnLvnItemchangedList1)
	ON_BN_CLICKED(IDOK, &CWSLTuxDlg::OnBnClickedOk)
	ON_MESSAGE(UWM_TRAY_MESSAGE, OnTrayNotify)
	ON_BN_CLICKED(IDCANCEL, &CWSLTuxDlg::OnBnClickedCancel)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_STOP, &CWSLTuxDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_START, &CWSLTuxDlg::OnBnClickedStart)
	ON_WM_TIMER()
	ON_WM_WINDOWPOSCHANGING()
	ON_REGISTERED_MESSAGE(UWM_ARE_YOU_ME, &CWSLTuxDlg::OnAreYouMe)
	ON_WM_ENDSESSION()
END_MESSAGE_MAP()
#pragma warning( pop )

WSLInfo::~WSLInfo()
{
	if (wiMutex)
		CloseHandle(wiMutex);
}

void WSLInfo::init()
{
	clear();
	wiMutex = CreateMutex(NULL, FALSE, NULL);
}

void WSLInfo::clear()
{
	rows = 0;
	dists_running = 0;
	procs_running = 0;
	columns.clear();
	for (int i = 0; i < 10; ++i)
		vcol[i].clear();
	for (std::vector<wslDistribution>::iterator wdi = distributions.begin(); wdi != distributions.end(); ++wdi)
		wdi->clear();
}

bool WSLInfo::lock()
{
	DWORD dwCount = 0, dwWaitResult;

	while (dwCount < 20)
	{
		dwWaitResult = WaitForSingleObject(wiMutex, 5000);
		switch (dwWaitResult)
		{
			case WAIT_OBJECT_0:
				++dwCount;
				return true;
			case WAIT_ABANDONED:
				return false;
		}
	}

	return true;
}

bool WSLInfo::unlock()
{
	if (ReleaseMutex(wiMutex))
		return true;
	return false;
}


LRESULT CWSLTuxDlg::OnAreYouMe(WPARAM, LPARAM)
{
	return m_initialized ? UWM_ARE_YOU_ME : 0;
} // CWSLTuxDlg::OnAreYouMe

BOOL CALLBACK CWSLTuxDlg::searcher(HWND hWnd, LPARAM lParam)
{
	DWORD_PTR result;
	LRESULT ok = ::SendMessageTimeout(hWnd,
		UWM_ARE_YOU_ME,
		0, 0,
		SMTO_BLOCK |
		SMTO_ABORTIFHUNG,
		200,
		&result);
	if (ok == 0)
		return TRUE; // ignore this and continue
	if (result == UWM_ARE_YOU_ME)
	{ /* found it */
		HWND* target = (HWND*)lParam;
		*target = hWnd;
		return FALSE; // stop search
	} /* found it */
	return TRUE; // continue search
} // CMyApp::searcher

#if 1
// This is used to run an external program, typically WSL.EXE
// The output of the program will be written to this->ProgramOutput
// A window will not be displayed for the application
HRESULT CWSLTuxDlg::RunExternalProgram(CString cmd)
{
	StopTimer();
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES saAttr;
	std::string cmdline = CT2CA(cmd);
	DWORD dwExitCode = 0;
	DWORD timeout = 10000; // timeout after ten seconds

	ZeroMemory(&saAttr, sizeof(saAttr));
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 
	if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0))
	{
		// log error
		StartTimer();
		return HRESULT_FROM_WIN32(GetLastError());
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
	{
		// log error
		StartTimer();
		return HRESULT_FROM_WIN32(GetLastError());
	}

	ProgramOutput = _T("");
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.hStdError = m_hChildStd_OUT_Wr;
	si.hStdOutput = m_hChildStd_OUT_Wr;
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	ZeroMemory(&pi, sizeof(pi));

	// Start the child process. 
	if (!CreateProcessA(NULL,		// No module name (use command line)
		(LPSTR)cmdline.c_str(),		// Command line
		NULL,					// Process handle not inheritable
		NULL,						// Thread handle not inheritable
		TRUE,						// Set handle inheritance
		0,							// No creation flags
		NULL,						// Use parent's environment block
		NULL,						// Use parent's starting directory 
		(LPSTARTUPINFOA) & si,					// Pointer to STARTUPINFO structure
		&pi)					// Pointer to PROCESS_INFORMATION structure
		)
	{
		AfxMessageBox(_T("Error: Failed to create child process"));
		StartTimer();
		return HRESULT_FROM_WIN32(GetLastError());
	}

	// wait for the process to complete within the timeout, and terminate it if it is taking too long
	if (WaitForSingleObject(pi.hProcess, timeout) == WAIT_TIMEOUT)
		TerminateProcess(pi.hProcess, dwExitCode);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(m_hChildStd_OUT_Wr);

	DWORD dwRead;
	CHAR chBuf[1];
	BOOL bSuccess = FALSE;

	// read the program's output into the buffer
	for (;;)
	{
		bSuccess = ReadFile(m_hChildStd_OUT_Rd, chBuf, 1, &dwRead, NULL);
		if (bSuccess != 1 || dwRead == 0) break;
		if (chBuf[0]) ProgramOutput += chBuf[0];
	}

	CloseHandle(m_hChildStd_OUT_Rd);
	StartTimer(1000);

	return S_OK;
}
#endif

bool TerminateProcessID(DWORD processID)
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS | PROCESS_TERMINATE, FALSE, processID);

	if (!hProcess)
	{
#ifdef _DEBUG
		OutputDebugString(_T("OpenProcess failed\n"));
#endif
		return false;
	}

	bool terminated = TerminateProcess(hProcess, 0);

#ifdef _DEBUG
	if ( !terminated )
	OutputDebugString(_T("TerminateProcess failed\n"));
#endif

	CloseHandle(hProcess);

	return terminated;
}

#if 0
// Terminate a distribution by terminating all processes associated with it
CString CWSLTuxDlg::WSLstopDistribution(CString& distro)
{
	std::vector<wslDistribution>::iterator wdi;
	CString msg;

	for (wdi = wslinfo.distributions.begin(); wdi != wslinfo.distributions.end(); ++wdi)
		if (wdi->name == distro)
			break;

	if (wdi == wslinfo.distributions.end())
	{
		msg.Format(_T("Distribution '%s' not found."), distro.GetString());
		return msg;
	}

	if ( wdi->pids.empty() )
	{
		msg.Format(_T("Distribution '%s' not running."), distro.GetString());
		return msg;
	}

	std::vector<DWORD>::iterator pi;
	int cnt = 0;

	for (pi = wdi->pids.begin(); pi != wdi->pids.end(); ++pi)
		if (TerminateProcessID(*pi))
			++cnt;

	msg.Format(_T("Distribution '%s' %d processes terminated."), distro.GetString(), cnt);

	return msg;
}
#else
// Terminate a distribution using WSL.EXE, does not bring up a window
CString CWSLTuxDlg::WSLstopDistribution(CString& distro)
{
	CString cmd("\\Windows\\System32\\wsl.exe -t ");

	cmd.Append(distro);

	if (RunExternalProgram(cmd) != S_OK)
		return _T("Failed to execute command");

	if (ProgramOutput.GetLength() > 4)
		return ProgramOutput;

	cmd.Format(_T("Distribution '%s' stopped."), distro.GetString());

	return cmd;
}
#endif

DWORD WINAPI WSLstart(LPVOID lpParam)
{
	if (!_wslinfo)
	{
		AfxMessageBox(_T("_wslinfo null!"));
		return 0;
	}
	CString msg;
	size_t dist = (size_t)lpParam;

	if (dist >= _wslinfo->distributions.size())
	{
		msg.Format(_T("Distribution index out of bounds: %llu >= %llu"), dist, _wslinfo->distributions.size());
		AfxMessageBox(msg);
		return 0;
	}

	WslApiLoader g_wslApi(_wslinfo->distributions[dist].wsname);
	static DWORD exitCode;
	HRESULT hin = g_wslApi.WslLaunchInteractive(NULL, false, &exitCode);

	ExitThread(hin);

	return 0;
}

#if USE_WSLLAUNCHINTERACTIVE
// Start a distribution using WSL.EXE, this should bring up a terminal window
CString CWSLTuxDlg::WSLstartDistribution(CString& distro)
{
	StopTimer();
	HRESULT hin = S_OK;
	HANDLE tid;
	DWORD dwTid;
	size_t dist;
	CString ret = _T("Test complete");

	if (!wslinfo.lock())
	{
		ret = _T("Failed to lock");
		return ret;
	}

	for (dist = 0; dist < wslinfo.distributions.size(); ++dist)
		if (wslinfo.distributions[dist].name == distro)
			break;

	if ( dist >= wslinfo.distributions.size() )
	{
		ret.Format(_T("Cannot find distribution '%s' in distribution list"), distro.GetString());
		StartTimer();
		wslinfo.unlock();
		return ret;
	}

	tid = CreateThread(NULL, 0, WSLstart, (LPVOID)dist, 0, &dwTid);
	Sleep(100); // give the thread time to access wslinfo before unlocking
	wslinfo.unlock();

	if (!tid)
	{
		ret.Format(_T("Distribution '%s' CreateThread failed!"), distro.GetString());
		StartTimer();
		return ret;
	}

	if (hin == S_OK)
	{
		ret.Format(_T("Distribution '%s' started."), distro.GetString());
		StartTimer(1000);  // refresh sooner
		return ret;
	}

	StartTimer();

	ret.Format(_T("Distribution '%s' failed to start: "), distro.GetString());
	switch (hin)
	{
		default:
			ret.Append(_T("Unspecified failure"));
			break;
		case E_NOTIMPL:
			ret.Append(_T("Not implemented"));
			break;
		case E_NOINTERFACE:
			ret.Append(_T("No such interface supported"));
			break;
		case E_POINTER:
			ret.Append(_T("Invalid reference"));
			break;
		case E_ABORT:
			ret.Append(_T("Operation aborted"));
			break;
		case E_FAIL:
			ret.Append(_T("Unspecified failure"));
			break;
		case E_UNEXPECTED:
			ret.Append(_T("Unexpected failure"));
			break;
		case E_ACCESSDENIED:
			ret.Append(_T("Access denied"));
			break;
		case E_HANDLE:
			ret.Append(_T("Invalid handle"));
			break;
		case E_OUTOFMEMORY:
			ret.Append(_T("Out of memory"));
			break;
		case E_INVALIDARG:
			ret.Append(_T("Invalid argument"));
			break;
	}

	return ret;
}
#else
// Start a distribution using WSL.EXE, this should bring up a terminal window
CString CWSLTuxDlg::WSLstartDistribution(CString& distro)
{
	CString cmd("C:\\Windows\\System32\\wsl.exe");
	CString params("-d ");
	params.Append(distro);

	HINSTANCE hin = ShellExecute(NULL, _T("open"), cmd, params, NULL, SW_SHOWNORMAL);

	if ((INT_PTR)hin > 32)
	{
		cmd.Format(_T("Distribution '%s' started."), distro.GetString());
		return cmd;
	}

	cmd.Format(_T("Distribution '%s' failed to start: "), distro.GetString());
	switch ((INT_PTR)hin)
	{
	default:
		cmd.Append(_T("OS error"));
		break;
	case ERROR_FILE_NOT_FOUND:
		cmd.Append(_T("WSL.EXE not found"));
		break;
	case ERROR_BAD_FORMAT:
		cmd.Append(_T("WSL.EXE is invalid"));
		break;
	case SE_ERR_ACCESSDENIED:
		cmd.Append(_T("Access Denied"));
		break;
	case SE_ERR_OOM:
		cmd.Append(_T("Out of Memory"));
		break;
	case SE_ERR_SHARE:
		cmd.Append(_T("Sharing Violation"));
		break;
	}

	return cmd;
}
#endif


// Populate the WSLInfo (this->wslinfo) data structure
bool CWSLTuxDlg::GetWSLInfo()
{
#ifdef _DEBUG
	OutputDebugString(_T("GetWSLInfo() START\n"));
#endif

	// Get the list of distributions from the registry
	if (!GetDistributionList())
	{
#ifdef _DEBUG
		OutputDebugString(_T("GetDistributionList() failed!\n"));
#endif
		wslinfo.unlock();
		return false;
	}

	wslinfo.clear();
#if 0
	// Get which distributions are running
	if (!GetDistributionStates())
	{
#ifdef _DEBUG
		OutputDebugString(_T("GetDistributionStates() failed!\n"));
#endif
		wslinfo.unlock();
		return false;
	}
#endif
	// Walk the process list for wslhost.exe processes matching distributions
	if (!GetDistributionProcs())
	{
#ifdef _DEBUG
		OutputDebugString(_T("GetDistributionProcs() failed!\n"));
#endif
		wslinfo.unlock();
		return false;
	}

	return true;
}


// (Re)populate the CListCtrl list of WSL distributions and their states
void CWSLTuxDlg::PopulateWSLlist()
{
	CString cstr = _T(""), ctmp;
	int i = 0;
	std::vector<CString>::iterator vsi, vsj;
	std::vector<wslDistribution>::iterator wdi;
	int nIndex, oIndex;

	oIndex = m_WSLlistCtrl.GetSelectionMark();
#ifdef _DEBUG
	cstr.Format(_T("GetSelectionMark: %d\n"), oIndex);
	OutputDebugString(cstr.GetString());
#endif


	if (m_WSLlistCtrl.DeleteAllItems() == 0)
	{
#ifdef _DEBUG
		OutputDebugString(_T("DeleteAllItems Failed!"));
#endif
	}

	for (wdi = wslinfo.distributions.begin(); wdi != wslinfo.distributions.end(); ++wdi)
	{
		// set * if we match default distribution
		if (!lsxxDefaultDistribution.Compare(wdi->regkey))
		{
			cstr = _T("*");
			DefaultDistribution = wdi->name;
		}
		else
			cstr = _T("");
		nIndex = m_WSLlistCtrl.InsertItem(i, cstr);
		m_WSLlistCtrl.SetItemText(nIndex, 1, wdi->name);
		if (wdi->pids.size())
		{
			cstr.Format(_T("Running (%llu)"), wdi->pids.size());
			m_WSLlistCtrl.SetItemText(nIndex, 2, cstr);
			//			m_WSLlistCtrl.SetItemText(nIndex, 2, _T("Running"));
		}
		else
			m_WSLlistCtrl.SetItemText(nIndex, 2, _T("Stopped"));

		cstr.Format(_T("%d"), wdi->version);
		m_WSLlistCtrl.SetItemText(nIndex, 3, cstr);
	}

	if (oIndex != -1)
	{
		m_WSLlistCtrl.SetSelectionMark(oIndex);
		m_WSLlistCtrl.SetItemState(oIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	}
}

void CWSLTuxDlg::StopTimer()
{
	if (m_minuteTimer)
		KillTimer(m_minuteTimer);
}


void CWSLTuxDlg::StartTimer(UINT interval)
{
	m_minuteTimer = SetTimer(IDT_MINUTE_TIMER, interval, NULL);
}


void CWSLTuxDlg::RestartTimer()
{
	StopTimer();
	StartTimer();
}

void CWSLTuxDlg::StartClickTimer()
{
	m_clickTimer = SetTimer(IDT_CLICK_TIMER, CLICK_INTERVAL, NULL);
#ifdef _DEBUG
	OutputDebugString(L"StartClickTimer\n");
#endif
}

void CWSLTuxDlg::StopClickTimer()
{
	if (m_clickTimer)
	{
#ifdef _DEBUG
		OutputDebugString(L"StopClickTimer -- killing timer\n");
#endif
		KillTimer(m_clickTimer);
	}
#ifdef _DEBUG
	else
	{
		OutputDebugString(L"StopClickTimer\n");
	}
#endif
	m_clicks = 0;
}



class MyRegInfo
{
public:
	HKEY	 hKey;							// key reference
	WCHAR    achKey[MAX_KEY_LENGTH];		// buffer for subkey name
	DWORD    cbName = MAX_KEY_LENGTH;		// size of name string
	WCHAR    achClass[MAX_PATH];			// buffer for class name
	DWORD    cchClassName = MAX_PATH;		// size of class string
	DWORD    cSubKeys;						// number of subkeys
	DWORD    cbMaxSubKey;					// longest subkey size
	DWORD    cchMaxClass;					// longest class string
	DWORD    cValues;						// number of values for key
	DWORD    cchMaxValue;					// longest value name
	DWORD    cbMaxValueData;				// longest value data
	DWORD    cbSecurityDescriptor;			// size of security descriptor
	FILETIME ftLastWriteTime;				// last write time
	DWORD	 retCode;
	WCHAR*	 achValue;
	DWORD	 cchValue;
	DWORD	 dwValue;
	DWORD	 dwIndex;
	DWORD	 lpType;

	void init()
	{
		achKey[0] = '\0';
		achClass[0] = '\0';
		achValue[0] = '\0';
		cbName = MAX_KEY_LENGTH;
		cSubKeys = 0;
		cbMaxSubKey = 0;
		cchMaxClass = 0;
		cValues = 0;
		cchMaxValue = 0;
		cbMaxValueData = 0;
		cbSecurityDescriptor = 0;
		retCode = 0;
		cchValue = MAX_VALUE_NAME;
		dwIndex = 0;
		dwValue = 0;
		lpType = 0;
		hKey = 0;
	}

	MyRegInfo(HKEY hKeyRoot, LPCWSTR lpSubKey)
	{
		achValue = (WCHAR *)malloc(MAX_VALUE_NAME);
		init();

		if ( (retCode=::RegOpenKeyEx(HKEY_CURRENT_USER, lpSubKey, 0, KEY_READ, &hKey)) != ERROR_SUCCESS)
			return;

		// Get the class name and the value count.
		retCode = RegQueryInfoKey(
			hKey,                    // key handle
			achClass,                // buffer for class name
			&cchClassName,           // size of class string
			NULL,                    // reserved
			&cSubKeys,               // number of subkeys
			&cbMaxSubKey,            // longest subkey size
			&cchMaxClass,            // longest class string
			&cValues,                // number of values for this key
			&cchMaxValue,            // longest value name
			&cbMaxValueData,         // longest value data
			&cbSecurityDescriptor,   // security descriptor
			&ftLastWriteTime);       // last write time
	}

	~MyRegInfo()
	{
		if ( achValue )
			free(achValue);
		if ( hKey )
			::RegCloseKey(hKey);
	}

	DWORD EnumKey(DWORD i)
	{
		cbName = MAX_KEY_LENGTH;
		achKey[0] = '\0';
		return RegEnumKeyEx(hKey, i, achKey, &cbName, NULL, NULL, NULL, &ftLastWriteTime);
	}

	DWORD EnumValue(DWORD i)
	{
		cchValue = MAX_VALUE_NAME;
		achValue[0] = '\0';
		return RegEnumValue(hKey, i, achValue, &cchValue, NULL, NULL, NULL, NULL);
	}

	DWORD QueryValueSZ(CString keyname)
	{
		cchValue = MAX_VALUE_NAME;
		achValue[0] = '\0';

		return RegQueryValueEx(hKey, keyname, NULL, &lpType, (LPBYTE)achValue, &cchValue);
	}

	DWORD QueryValueDW(CString keyname)
	{
		cchValue = MAX_VALUE_NAME;
		dwValue = 0;

		return RegQueryValueEx(hKey, keyname, NULL, &lpType, (LPBYTE)&dwValue, &cchValue);
	}

	WCHAR* key()
	{
		return achKey;
	}

	DWORD dwvalue()
	{
		if (lpType != REG_DWORD)
			return 0;

		return dwValue;
	}

	WCHAR* szvalue()
	{
		if (lpType != REG_SZ)
			return NULL;

		return achValue;
	}
};

// Retrieve the WSL (lxss) distributions from the Windows Registry
// (Registry lookup functions appear to be fast and efficient)
bool CWSLTuxDlg::GetDistributionList()
{
	CString LxssKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Lxss");
#ifdef _DEBUG
	CString DbgString;
#endif
	DWORD i;

	MyRegInfo reginfo(HKEY_CURRENT_USER, LxssKey);

	if ( reginfo.retCode != ERROR_SUCCESS )
	{
#ifdef _DEBUG
		OutputDebugString(_T("RegOpenKeyEx LxssKey failed!\n"));
#endif
		return false;
	}

	// get default distribution and version
	if (reginfo.cValues && (reginfo.QueryValueSZ(L"DefaultDistribution") == ERROR_SUCCESS))
	{
		lsxxDefaultDistribution = reginfo.szvalue();
#ifdef _DEBUG
		DbgString.Format(_T("DefaultDistribution: %s\n"), lsxxDefaultDistribution.GetString());
		OutputDebugString(DbgString.GetString());
#endif
		if (reginfo.QueryValueDW(L"DefaultVersion") == ERROR_SUCCESS)
			lsxxDefaultVersion = reginfo.dwvalue();
	}
#ifdef _DEBUG
	else
		OutputDebugString(_T("Could not find default distribution\n"));
#endif
	wslinfo.distributions.clear();

	// build distribution list
	if ( reginfo.cSubKeys )
	{
#ifdef _DEBUG
		DbgString.Format(_T("\nNumber of subkeys : % d\n"), reginfo.cSubKeys);
		OutputDebugString(DbgString);
#endif
		for (i = 0; i < reginfo.cSubKeys; i++)
		{
			if ( reginfo.EnumKey(i) == ERROR_SUCCESS )
			{
				CString csDistKey = LxssKey + "\\" + reginfo.key();
#ifdef _DEBUG
				DbgString.Format(_T("(%d) distribution: %s\n"), i + 1, reginfo.key());
				OutputDebugString(DbgString.GetString());
#endif
				MyRegInfo distinfo(HKEY_CURRENT_USER,csDistKey);

				if ( distinfo.retCode != ERROR_SUCCESS )
				{
#ifdef _DEBUG
					OutputDebugString(_T("Failed to open dist key\n"));
#endif
					continue;
				}

				if (!distinfo.cValues)
				{
#ifdef _DEBUG
					OutputDebugString(_T("no cvalues\n"));
#endif
					continue;
				}

				if ( distinfo.QueryValueSZ(L"DistributionName") != ERROR_SUCCESS )
				{
#ifdef _DEBUG
					OutputDebugString(_T("Failed to get distribution name\n"));
#endif
					continue;
				}

				wslDistribution dist;
#ifdef _DEBUG
				DbgString.Format(_T("... DistributionName: %s\n"), distinfo.szvalue());
				OutputDebugString(DbgString.GetString());
#endif
				dist.regkey = reginfo.key();
				dist.name = distinfo.szvalue();

				if (distinfo.QueryValueDW(L"Version") == ERROR_SUCCESS)
					dist.version = distinfo.dwvalue();
				if (distinfo.QueryValueDW(L"Flags") == ERROR_SUCCESS)
					dist.flags = distinfo.dwvalue();
				if (distinfo.QueryValueDW(L"DefaultUid") == ERROR_SUCCESS)
					dist.uid = distinfo.dwvalue();
				if (distinfo.QueryValueSZ(L"PackageFamilyName") == ERROR_SUCCESS)
					dist.packagefamilyname = distinfo.szvalue();
				if (distinfo.QueryValueSZ(L"BasePath") == ERROR_SUCCESS)
					dist.basepath = distinfo.szvalue();

				wslinfo.distributions.push_back(dist);
			}
		}
	}
#ifdef _DEBUG
	else
		OutputDebugString(_T("Could not find any distributions!\n"));
#endif
	return true;
}

// check if a process is wslhost.exe, and add the full commandline to wslhosts
// this operation takes the most resources of all, so we only want to perform
// this every few seconds
int AddWSLhost(DWORD processID, std::map<DWORD, CString>& wslhosts)
{
	TCHAR szProcessName[MAX_PATH] = { 0 };
#ifdef _DEBUG
	CString dbg;
#endif

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

	if (!hProcess)
	{
#if 0 //def _DEBUG
		dbg.Format(_T("OpenProcess(%d) failed\n"), processID);
		OutputDebugString(dbg.GetString());
#endif
		return -1;
	}

	HMODULE hMod;
	DWORD cbNeeded;

	if (!EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
	{
#ifdef _DEBUG
		dbg.Format(_T("EnumProcessModules(%d) failed\n"), processID);
		OutputDebugString(dbg.GetString());
#endif
		CloseHandle(hProcess);
		return -1;
	}

	GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
	// must be a wslhost.exe process
	if (_wcsicmp(szProcessName,L"wslhost.exe"))
	{
		//	dbg.Format(_T("'%s' not wslhost.exe\n"), szProcessName);
		//	OutputDebugString(dbg.GetString());
		CloseHandle(hProcess);
		return 0;
	}
#ifdef _DEBUGXX
	OutputDebugString(_T("Got wslhost.exe!\n"));
#endif
	PROCESS_BASIC_INFORMATION pbi;
	NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);

	if (!NT_SUCCESS(status))
	{
#ifdef _DEBUG
		dbg.Format(_T("NtQueryInfomationProcess Failed\n"));
		OutputDebugString(dbg.GetString());
#endif
		CloseHandle(hProcess);
		return -1;
	}

	RTL_USER_PROCESS_PARAMETERS upp;
	SIZE_T dwSize;
	PEB peb;

	ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb, sizeof(PEB), (SIZE_T*)&dwSize);
	ReadProcessMemory(hProcess, peb.ProcessParameters, &upp, sizeof(RTL_USER_PROCESS_PARAMETERS), (SIZE_T*)&dwSize);

	SIZE_T cmdbuflen = upp.CommandLine.Length;
	WCHAR* CmdLine = (WCHAR*)malloc(cmdbuflen);

	if (!CmdLine)
	{
		CloseHandle(hProcess);
		return -1;
	}

	if (!ReadProcessMemory(hProcess, upp.CommandLine.Buffer, CmdLine, upp.CommandLine.Length, NULL))
	{
		free(CmdLine);
		CloseHandle(hProcess);
		return -1;
	}

	std::wstring wcmd(CmdLine, cmdbuflen / 2);
	//std::wstring_convert<ccvt> wstrtostr;
	//std::string cmd = wstrtostr.to_bytes(wcmd);
	CString cmd(wcmd.c_str());

	wslhosts.insert(std::pair<DWORD,CString>(processID, cmd));

	free(CmdLine);

	CloseHandle(hProcess);

	return 1;
}

// build vector of wslhost.exe process command lines
int GetWSLhosts(std::map<DWORD, CString>& wslhosts)
{
	// Get the list of process identifiers.

	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;

	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
		return -1;

	cProcesses = cbNeeded / sizeof(DWORD);

	wslhosts.clear();

	for (i = 0; i < cProcesses; i++)
		if (aProcesses[i])
			AddWSLhost(aProcesses[i], wslhosts);
#ifdef _DEBUG
	CString dbg;
	dbg.Format(_T("GetWSLhosts() total processes: %d\n"), cProcesses);
	OutputDebugString(dbg.GetString());
#endif
	return 0;
}

// saw this technique in LxRunOffline, but it doesn't seem to work reliably
bool distribution_is_running(CString& basepath)
{
	CString vfs = basepath;
	CString msg;

	if (vfs[0] == '\\' && vfs[1] == '\\')
		vfs = vfs.Mid(4);

	vfs.Append(L"\\ext4.vhdx");

	HANDLE hFile = CreateFile(vfs, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();

//		msg.Format(_T("CreateFile(%s) failed with err %d"), vfs, err);
//		AfxMessageBox(msg);

		switch (err)
		{
			default:
				break;
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return false;
		}

		return true;
	}

	CloseHandle(hFile);

	return false;
}

// check which distributions are currently running 
bool CWSLTuxDlg::GetDistributionStates()
{
	std::vector<wslDistribution>::iterator wdi;
	CString msg;

	for (wdi = wslinfo.distributions.begin(); wdi != wslinfo.distributions.end(); ++wdi)
		wdi->running = distribution_is_running(wdi->basepath);

	return true;
}


// search for wslhost.exe processes with --distro-id matching distributions
// to build a list of associated process ids
bool CWSLTuxDlg::GetDistributionProcs()
{
	std::map<DWORD, CString> wslhosts;
	std::map<DWORD, CString>::iterator wi;
	std::vector<wslDistribution>::iterator wdi;
#ifdef _DEBUG
	CString dbg;
#endif
	if (GetWSLhosts(wslhosts) == -1)
	{
#ifdef _DEBUG
		OutputDebugString(_T("GetWSLhosts() failed!\n"));
#endif
		return false;
	}
#ifdef _DEBUG
	dbg.Format(_T("wslhosts.size() %llu\n"), wslhosts.size());
	OutputDebugString(dbg.GetString());
#endif

	// clear distribution pid lists
	for (wdi = wslinfo.distributions.begin(); wdi != wslinfo.distributions.end(); ++wdi)
		wdi->pids.clear();

	// grab pids matching distributions
	for (wi = wslhosts.begin(); wi != wslhosts.end(); ++wi)
	{
#ifdef _DEBUG
		dbg.Format(_T("Evaluating wslhost %d: %s\n"), wi->first, wi->second.GetString());
		OutputDebugString(dbg.GetString());
#endif
		for (wdi = wslinfo.distributions.begin(); wdi != wslinfo.distributions.end(); ++wdi)
		{
			if (wi->second.Find(wdi->regkey) != -1)
			{
#ifdef _DEBUG
				dbg.Format(_T("Found %s in %s\n"), wdi->regkey.GetString(), wi->second.GetString());
				OutputDebugString(dbg.GetString());
#endif
				wdi->pids.push_back(wi->first);
			}
		}
	}

	// set num running
	for (wdi = wslinfo.distributions.begin(); wdi != wslinfo.distributions.end(); ++wdi)
	{
		if (wdi->pids.size())
		{
			++wslinfo.dists_running;
			wslinfo.procs_running += wdi->pids.size();
		}
	}
	return true;
}


void CWSLTuxDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWSLTuxDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CWSLTuxDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CWSLTuxDlg::OnLvnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	int selectedItem = pNMLV->iItem;
	CString msg;
	msg = m_WSLlistCtrl.GetItemText(selectedItem, 1);
	*pResult = 0;
	CWnd* label = GetDlgItem(IDC_STATIC2);
	label->SetWindowTextW(msg);
	RestartTimer();
}

void CWSLTuxDlg::AddIconToSysTray()
{
	if (!wslinfo.lock())
		return;

	NOTIFYICONDATA NID;
	std::wstringstream oss;

	memset(&NID, 0, sizeof(NID));

	//on main function:
	NID.cbSize = sizeof(NID);
	if (wslinfo.dists_running)
		NID.hIcon = this->m_hIcon;
	else
		NID.hIcon = this->m_hIcon_disabled;

	NID.hWnd = this->m_hWnd;
	NID.uID = UWM_TRAY_ID;
	if (wslinfo.dists_running == 1)
		oss << "1 distribution running\n" << wslinfo.procs_running << " processes running";
	else
		oss << wslinfo.dists_running << " distributions running\n" << wslinfo.procs_running << " processes running";

	wcscpy_s(NID.szTip, oss.str().c_str());
	NID.uCallbackMessage = UWM_TRAY_MESSAGE;
	//in a timer:

	NID.uFlags = NID.uFlags | NIF_ICON | NIF_TIP | NIF_MESSAGE;
	Shell_NotifyIcon(NIM_ADD, &NID);

	wslinfo.unlock();
	//CDialogEx::OnOK();
}

void CWSLTuxDlg::UpdateSysTrayIcon()
{
	if (!wslinfo.lock())
		return;

	NOTIFYICONDATA NID;
	std::wstringstream oss;

	memset(&NID, 0, sizeof(NID));

	NID.cbSize = sizeof(NID);
	if (wslinfo.dists_running)
		NID.hIcon = this->m_hIcon;
	else
		NID.hIcon = this->m_hIcon_disabled;

	NID.hWnd = this->m_hWnd;
	NID.uID = UWM_TRAY_ID;
	if (wslinfo.dists_running == 1)
		oss << "1 distribution running\n" << wslinfo.procs_running << " processes running";
	else
		oss << wslinfo.dists_running << " distributions running\n" << wslinfo.procs_running << " processes running";

	wcscpy_s(NID.szTip, oss.str().c_str());

	NID.uCallbackMessage = UWM_TRAY_MESSAGE;

	NID.uFlags = NID.uFlags | NIF_ICON | NIF_TIP | NIF_MESSAGE;
	Shell_NotifyIcon(NIM_MODIFY, &NID);
	wslinfo.unlock();
}

void CWSLTuxDlg::RemIconFromSysTray()
{
	NOTIFYICONDATA NID;

	memset(&NID, 0, sizeof(NID));
	NID.hWnd = this->m_hWnd;
	NID.uID = UWM_TRAY_ID;
	Shell_NotifyIcon(NIM_DELETE, &NID);
}


void CAboutDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	CDialogEx::OnOK();
}

void CWSLTuxDlg::RefreshWSLInfo()
{
	if (!wslinfo.lock())
		return;
	StopTimer();

	size_t prev_running = wslinfo.dists_running;

	if (GetWSLInfo())
	{
		// only repopulate the dialog list if it is visible
		if ( m_visible )
			PopulateWSLlist();
		if (wslinfo.dists_running != prev_running)
			UpdateSysTrayIcon();
	}

	wslinfo.unlock();
	StartTimer();
}

// Refresh button
void CWSLTuxDlg::OnBnClickedOk()
{
	m_visible = true;
	RefreshWSLInfo();
}

// Cancel button
void CWSLTuxDlg::OnBnClickedCancel()
{
	HideMyWindow();
}

// Handle events from interacting with the system tray icon
LRESULT CWSLTuxDlg::OnTrayNotify(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);

	UINT uMsg = (UINT)lParam;
	const int IDM_EXIT = 100;

	switch (uMsg)
	{

		case WM_LBUTTONDOWN:
			if (++m_clicks == 1)
				StartClickTimer();
			break;
		case WM_LBUTTONDBLCLK:
			StopClickTimer();
			WSLstartDistribution(DefaultDistribution);
			break;
		case WM_CONTEXTMENU:
		case WM_RBUTTONDOWN:
		{
			POINT pt;

			GetCursorPos(&pt);
			
			HMENU hmenu = CreatePopupMenu();
			InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");
			SetMenuDefaultItem(hmenu, IDM_EXIT, FALSE);
			//SetForegroundWindow();
			SetFocus();
			//SendMessage(WM_INITMENUPOPUP, (WPARAM)hmenu, 0);
			int cmd = TrackPopupMenu(hmenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, this->m_hWnd, NULL);
			DestroyMenu(hmenu);
			if (cmd == IDM_EXIT)
			{
				CWSLTuxDlg::Shutdown();
			}
			break;
		}
	}
	return 0;
}

// Shutdown the application
void CWSLTuxDlg::Shutdown()
{
	RemIconFromSysTray();
	CDialogEx::OnCancel();
}

// Clicking the X in corner just hides the window
void CWSLTuxDlg::OnClose()
{
	HideMyWindow();
	CDialogEx::OnClose();
}


// Stop a distribution from running
void CWSLTuxDlg::OnBnClickedStop()
{
	POSITION pos = m_WSLlistCtrl.GetFirstSelectedItemPosition();

	if (!pos)
	{
		AfxMessageBox(_T("No distribution selected"));
		return;
	}

	int nItem = m_WSLlistCtrl.GetNextSelectedItem(pos);
	CString distro = m_WSLlistCtrl.GetItemText(nItem, 1);
	CString prompt;

	prompt.Format(_T("Stop %s. Are you sure?"), distro.GetString());

	if (AfxMessageBox(prompt, MB_YESNO) != IDYES)
		return;

	prompt = WSLstopDistribution(distro);

	RefreshWSLInfo();

	AfxMessageBox(prompt);
}


// Start a distribution
void CWSLTuxDlg::OnBnClickedStart()
{
	POSITION pos = m_WSLlistCtrl.GetFirstSelectedItemPosition();

	if (!pos)
	{
		AfxMessageBox(_T("No distribution selected"));
		return;
	}

	int nItem = m_WSLlistCtrl.GetNextSelectedItem(pos);
	std::vector<wslDistribution>::iterator wdi;
	CString distro = m_WSLlistCtrl.GetItemText(nItem, 1);
	CString prompt;

	for (wdi = wslinfo.distributions.begin(); wdi != wslinfo.distributions.end(); ++wdi)
		if (wdi->name == distro)
			break;

	if (wdi == wslinfo.distributions.end())
		return;

	if (wdi->pids.size())
		prompt.Format(_T("%s is already running. Do want to to start another session?"), distro.GetString());
	else
		prompt.Format(_T("Start %s. Are you sure?"), distro.GetString());

	if (AfxMessageBox(prompt, MB_YESNO) != IDYES)
		return;

	prompt = WSLstartDistribution(distro);

	RefreshWSLInfo();

//	AfxMessageBox(prompt);
}


void CWSLTuxDlg::ShowMyWindow()
{
	m_visible = true;
	this->ShowWindow(SW_RESTORE);
	this->ShowWindow(SW_SHOW);
	this->BringWindowToTop();
	this->SetForegroundWindow();
}

void CWSLTuxDlg::HideMyWindow()
{
	m_visible = false;
	this->ShowWindow(SW_HIDE);
}


// timer event to refresh the app information
void CWSLTuxDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (!KillTimer(nIDEvent))
		return;

	if (nIDEvent == IDT_CLICK_TIMER)
	{
		m_clicks = 0;
		ShowMyWindow();
		return;
	}

	if (nIDEvent == IDT_MINUTE_TIMER)
	{
		RefreshWSLInfo();
		SetTimer(nIDEvent, TIMER_INTERVAL, NULL);
	}
}

// Initialize primary dialog -- this is basically our main()
BOOL CWSLTuxDlg::OnInitDialog()
{
	// check if we are already running an instance
	// from http://www.flounder.com/nomultiples.htm
	bool AlreadyRunning;

	HANDLE hMutexOneInstance = ::CreateMutex(NULL, FALSE,
		_T("WSLTUX-9A4D70DF-D007-4057-B2F8-A163A199CEB9"));

	AlreadyRunning = (::GetLastError() == ERROR_ALREADY_EXISTS ||
		::GetLastError() == ERROR_ACCESS_DENIED);

	if (AlreadyRunning)
	{ /* kill this */
		HWND hOther = NULL;
		EnumWindows(searcher, (LPARAM)&hOther);

		if (hOther != NULL)
		{ /* pop up */

			// make sure we're not referencing ourself
			if (hOther != this->m_hWnd)
			{ /* restore */
				::ShowWindow(hOther, SW_RESTORE);
				::ShowWindow(hOther, SW_SHOW);
				::BringWindowToTop(hOther);
				::SetForegroundWindow(hOther);
			} /* restore */
		} /* pop up */

		CDialogEx::OnCancel();	// exit dialog

		return FALSE; // terminates the creation
	}

	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);		// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// put column headings into the ClistCtrl
	m_WSLlistCtrl.SetExtendedStyle(m_WSLlistCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	m_WSLlistCtrl.InsertColumn(0, _T(""), LVCFMT_LEFT, 20);
	m_WSLlistCtrl.InsertColumn(1, _T("Name"), LVCFMT_LEFT, 250);
	m_WSLlistCtrl.InsertColumn(2, _T("State"), LVCFMT_LEFT, 90);
	m_WSLlistCtrl.InsertColumn(3, _T("Version"), LVCFMT_LEFT, 60);

	if (GetWSLInfo() && wslinfo.distributions.size())
	{
		PopulateWSLlist();
		StartTimer();
	}

	AddIconToSysTray();

	m_initialized = true;

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// used to hide the app dialog on start
// from https://www.codeproject.com/Articles/1724/Some-handy-dialog-box-tricks-tips-and-workarounds
void CWSLTuxDlg::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
	if (!m_visible)
		lpwndpos->flags &= ~SWP_SHOWWINDOW;
	CDialogEx::OnWindowPosChanging(lpwndpos);
}


void CWSLTuxDlg::OnEndSession(BOOL bEnding)
{
#ifdef _DEBUG
	OutputDebugString(_T("CWSLTuxDlg::OnEndSession() shutting down\n"));
#endif
	CDialogEx::OnEndSession(bEnding);
	ExitProcess(0);
}
