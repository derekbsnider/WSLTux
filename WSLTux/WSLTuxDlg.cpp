
// WSLTuxDlg.cpp : implementation file
// 
// WSL Tux is a system tray application to let you know that WSL is running by placing a Tux icon in the system tray
// 
// (c)2022 Derek Snider (DSD Software)
//


#include "pch.h"
#include "framework.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include "WSLTux.h"
#include "WSLTuxDlg.h"
#include "afxdialogex.h"
#include "wslapi.h"
#include <io.h>
#include "console_pipe.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_TRAY_ID (WM_USER + 0x2)
#define WM_TRAY_MESSAGE (WM_USER + 0x100)
#define IDT_MINUTE_TIMER (WM_USER + 0x200)

#define TIMER_INTERVAL 60000

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
	ON_MESSAGE(WM_TRAY_MESSAGE, OnTrayNotify)
	ON_BN_CLICKED(IDCANCEL, &CWSLTuxDlg::OnBnClickedCancel)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_STOP, &CWSLTuxDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_START, &CWSLTuxDlg::OnBnClickedStart)
	ON_WM_TIMER()
END_MESSAGE_MAP()
#pragma warning( pop )

void getColumns(std::vector<CString>& columns, CString& cstr)
{
	std::string str = CT2A(cstr);
	CString tcstr;
	size_t pos, p2;

	pos = str.find_first_not_of(' ');
	tcstr = CString(str.substr(0, pos - 1).c_str());
	columns.push_back(tcstr);
	str = str.substr(pos);

	while ((pos = str.find_first_of(' ')) != std::string::npos)
	{
		p2 = str.find_first_not_of(' ', pos);
		if (p2 != std::string::npos)
			pos = p2;
		tcstr = CString(str.substr(0, pos - 1).c_str());
		columns.push_back(tcstr);
		str = str.substr(pos);
	}
	if (!str.empty())
	{
		tcstr = CString(str.c_str());
		columns.push_back(tcstr);
	}
}


void WSLInfo::clear()
{
	rows = 0;
	num_running = 0;
	columns.clear();
	for (int i = 0; i < 10; ++i)
		vcol[i].clear();
}



HRESULT CWSLTuxDlg::RunExternalProgram(CString cmd)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES saAttr;
	std::string cmdline = CT2CA(cmd);
	DWORD dwExitCode = 0;
	DWORD timeout = 30000;

	ZeroMemory(&saAttr, sizeof(saAttr));
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 

	if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0))
	{
		// log error
		return HRESULT_FROM_WIN32(GetLastError());
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.

	if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
	{
		// log error
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
		(LPSTR)cmdline.c_str(),			// Command line
		NULL,						// Process handle not inheritable
		NULL,						// Thread handle not inheritable
		TRUE,						// Set handle inheritance
		0,							// No creation flags
		NULL,						// Use parent's environment block
		NULL,						// Use parent's starting directory 
		(LPSTARTUPINFOA) & si,		// Pointer to STARTUPINFO structure
		&pi)						// Pointer to PROCESS_INFORMATION structure
		)
	{
		AfxMessageBox(_T("Failed to create child process"));
		return HRESULT_FROM_WIN32(GetLastError());
	}

	if (WaitForSingleObject(pi.hProcess, timeout) == WAIT_TIMEOUT)
		TerminateProcess(pi.hProcess, dwExitCode);
//	WaitForSingleObject(pi.hThread, INFINITE);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(m_hChildStd_OUT_Wr);

	DWORD dwRead;
	CHAR chBuf[1];
	BOOL bSuccess = FALSE;

	for (;;)
	{
		bSuccess = ReadFile(m_hChildStd_OUT_Rd, chBuf, 1, &dwRead, NULL);
		if (bSuccess != 1 || dwRead == 0) break;
		if (chBuf[0]) ProgramOutput += chBuf[0];
	}

	CloseHandle(m_hChildStd_OUT_Rd);

	return S_OK;
}

CString CWSLTuxDlg::WSLstopDistribution(CString& distro)
{
	CString cmd("\\Windows\\System32\\wsl.exe -t ");

	cmd.Append(distro);

	if ( RunExternalProgram(cmd) != S_OK )
		return _T("Failed to execute command");

	if (ProgramOutput.GetLength() > 4)
		return ProgramOutput;

	cmd.Format(_T("Distribution '%s' stopped."), distro.GetString());

	return cmd;
}

CString CWSLTuxDlg::WSLstartDistribution(CString& distro)
{
	CString cmd("\\Windows\\System32\\wsl.exe -d ");

	cmd.Append(distro);
	cmd.Append(_T(" uname -a"));

	if (RunExternalProgram(cmd) != S_OK)
		return _T("Failed to execute command");

	if (ProgramOutput.GetLength() > 4)
		return ProgramOutput;

	cmd.Format(_T("Distribution '%s' started."), distro.GetString());

	return cmd;
}


// CWSLTuxDlg message handlers

bool CWSLTuxDlg::GetWSLInfo()
{
	RunExternalProgram(_T("\\Windows\\System32\\wsl.exe -l -v"));
	std::string strProgramOutput = CT2CA(ProgramOutput);
	std::istringstream pPipe(strProgramOutput);
	CString cstr = _T(""), ctmp;
	CString dbgmsg;
	int col, ofs, i = 0;
	char ch;
	std::vector<CString>::iterator vsi, vsj;

	wslinfo.clear(); // reset wslinfo object

	while ( pPipe.get(ch) )
	{
		switch (ch)
		{
		case '\0': break;
		case '\n':
		case '\r':
			if (cstr.GetLength() > 0)
			{
				if (wslinfo.columns.empty())
				{
					getColumns(wslinfo.columns, cstr);
					cstr = _T("");
					continue;
				}

				for (col = 0, ofs = 0, vsi = wslinfo.columns.begin(); vsi != wslinfo.columns.end(); ++vsi)
				{
					ctmp = cstr.Mid(ofs, vsi->GetLength());
					ctmp = ctmp.Trim();
					if (vsi->Left(5).CompareNoCase(_T("STATE")) == 0
					&&  ctmp.CompareNoCase(_T("Running")) == 0)
						++wslinfo.num_running;

					wslinfo.vcol[col++].push_back(ctmp);
					ofs += vsi->GetLength();
				}
				cstr = _T("");
				++wslinfo.rows;
			}
			break;
		default:
			cstr += (char)ch;
		}
	}

	return wslinfo.rows > 0 ? true : false;
}

void CWSLTuxDlg::PopulateWSLlist()
{
	CString cstr = _T(""), ctmp;
	int col, i = 0;
	std::vector<CString>::iterator vsi, vsj;
	int nIndex;

	m_WSLlistCtrl.DeleteAllItems();

	for (i = 0; i < wslinfo.rows; ++i)
	{
		vsj = wslinfo.vcol[0].begin();
		if (vsj == wslinfo.vcol[0].end())
			break;
		nIndex = m_WSLlistCtrl.InsertItem(i, *vsj);
		wslinfo.vcol[0].erase(vsj);
		for (col = 1, vsi = wslinfo.columns.begin() + 1; vsi != wslinfo.columns.end(); ++vsi)
		{
			vsj = wslinfo.vcol[col].begin();
			if (vsj == wslinfo.vcol[col].end())
				continue;
			m_WSLlistCtrl.SetItemText(nIndex, col, *vsj);
			wslinfo.vcol[col].erase(vsj);
			++col;
		}
	}
}

BOOL CWSLTuxDlg::OnInitDialog()
{
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
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_WSLlistCtrl.SetExtendedStyle(m_WSLlistCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	m_WSLlistCtrl.InsertColumn(0, _T(""), LVCFMT_LEFT, 20);
	m_WSLlistCtrl.InsertColumn(1, _T("Name"), LVCFMT_LEFT, 250);
	m_WSLlistCtrl.InsertColumn(2, _T("State"), LVCFMT_LEFT, 90);
	m_WSLlistCtrl.InsertColumn(3, _T("Version"), LVCFMT_LEFT, 60);

	if (GetWSLInfo())
	{
		PopulateWSLlist();
		m_minuteTimer = SetTimer(IDT_MINUTE_TIMER, TIMER_INTERVAL, NULL);
	}

	AddIconToSysTray();

//	RunExternalProgram(_T("\\Windows\\System32\\wsl.exe -l -v"));
//	AfxMessageBox(ProgramOutput);

//	int nIndex = m_WSLlistCtrl.InsertItem(i++, _T(""));
//	m_WSLlistCtrl.SetItemText(nIndex, 1, cstr);

	return TRUE;  // return TRUE  unless you set the focus to a control
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
}

void CWSLTuxDlg::AddIconToSysTray()
{
	NOTIFYICONDATA NID;

	memset(&NID, 0, sizeof(NID));

	//on main function:
	NID.cbSize = sizeof(NID);
	if (wslinfo.num_running)
		NID.hIcon = this->m_hIcon;
	else
		NID.hIcon = this->m_hIcon_disabled;

	NID.hWnd = this->m_hWnd;
	NID.uID = WM_TRAY_ID;
	StrCpyW(NID.szTip, L"WSL Tux");
	NID.uCallbackMessage = WM_TRAY_MESSAGE;
	//in a timer:

	NID.uFlags = NID.uFlags | NIF_ICON | NIF_TIP | NIF_MESSAGE;
	Shell_NotifyIcon(NIM_ADD, &NID);

	//CDialogEx::OnOK();
}

void CWSLTuxDlg::UpdateSysTrayIcon()
{
	NOTIFYICONDATA NID;

	memset(&NID, 0, sizeof(NID));

	NID.cbSize = sizeof(NID);
	if (wslinfo.num_running)
		NID.hIcon = this->m_hIcon;
	else
		NID.hIcon = this->m_hIcon_disabled;

	NID.hWnd = this->m_hWnd;
	NID.uID = WM_TRAY_ID;
	StrCpyW(NID.szTip, L"WSL Tux");
	NID.uCallbackMessage = WM_TRAY_MESSAGE;

	NID.uFlags = NID.uFlags | NIF_ICON | NIF_TIP | NIF_MESSAGE;
	Shell_NotifyIcon(NIM_MODIFY, &NID);
}

void CWSLTuxDlg::RemIconFromSysTray()
{
	NOTIFYICONDATA NID;

	memset(&NID, 0, sizeof(NID));
	NID.hWnd = this->m_hWnd;
	NID.uID = WM_TRAY_ID;
	Shell_NotifyIcon(NIM_DELETE, &NID);
}


void CAboutDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	CDialogEx::OnOK();
}

void CWSLTuxDlg::RefreshWSLInfo()
{
	int prev_running = wslinfo.num_running;

	if (GetWSLInfo())
	{
		PopulateWSLlist();
		if (wslinfo.num_running != prev_running)
			UpdateSysTrayIcon();
	}
}

void CWSLTuxDlg::OnBnClickedOk()
{
	RefreshWSLInfo();
}

void CWSLTuxDlg::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here

	this->ShowWindow(SW_HIDE);
//	CDialogEx::OnCancel();
}


LRESULT CWSLTuxDlg::OnTrayNotify(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);

	UINT uMsg = (UINT)lParam;
	const int IDM_EXIT = 100;

	switch (uMsg)
	{
		case WM_LBUTTONDOWN:
			this->ShowWindow(SW_SHOW);
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

void CWSLTuxDlg::Shutdown()
{
	RemIconFromSysTray();
	CDialogEx::OnCancel();
}

void CWSLTuxDlg::OnClose()
{
	// TODO: Add your message handler code here and/or call default
	this->ShowWindow(SW_HIDE);
	CDialogEx::OnClose();
}


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


void CWSLTuxDlg::OnBnClickedStart()
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

	prompt.Format(_T("Start %s. Are you sure?"), distro.GetString());

	if (AfxMessageBox(prompt, MB_YESNO) != IDYES)
		return;

	prompt = WSLstartDistribution(distro);

	RefreshWSLInfo();

	AfxMessageBox(prompt);
}


void CWSLTuxDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (!KillTimer(nIDEvent))
		return;
	//	CDialogEx::OnTimer(nIDEvent);

	RefreshWSLInfo();

	SetTimer(nIDEvent, TIMER_INTERVAL, NULL);
}
