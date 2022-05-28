
// WSLTuxDlg.cpp : implementation file
// 
// WSL Tux is a system tray application to let you know that WSL is running by placing a Tux icon in the system tray
// 
// TODO: if no WSL distributions are active, then we should display a "grayed out" Tux icon
//		 also, we should poll the distributions periodically to see if they are active
//

#include "pch.h"
#include "framework.h"
#include "WSLTux.h"
#include "WSLTuxDlg.h"
#include "afxdialogex.h"
#include "wslapi.h"
#include <fstream>
#include <sstream>
#include <string>
#include <io.h>
#include "console_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_TRAY_ID (WM_USER + 0x2)
#define WM_TRAY_MESSAGE (WM_USER + 0x100)


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
}

void CWSLTuxDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_WSLlistCtrl);
}

BEGIN_MESSAGE_MAP(CWSLTuxDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, &CWSLTuxDlg::OnLvnItemchangedList1)
	ON_BN_CLICKED(IDOK, &CWSLTuxDlg::OnBnClickedOk)
	ON_MESSAGE(WM_TRAY_MESSAGE, OnTrayNotify)
	ON_BN_CLICKED(IDCANCEL, &CWSLTuxDlg::OnBnClickedCancel)
	ON_WM_CLOSE()
END_MESSAGE_MAP()

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


// CWSLTuxDlg message handlers

BOOL CWSLTuxDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	AddIconToSysTray();

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

	FILE* pPipe;

	// WSL.EXE outputs nulls for some strange reason
	// read the output from wsl.exe into a listbox
	if ((pPipe = _popen("\\Windows\\System32\\wsl.exe -l -v", "rb")))
	{
		CString cstr = _T(""), ctmp;
		int ch, col, ofs, rows = 0, i = 0;
		std::vector<CString>::iterator vsi, vsj;
		std::vector<CString> columns;
		std::vector<CString> vcol[10];
		int nIndex;

		while ((ch = getc(pPipe)) != EOF)
		{
			switch (ch)
			{
			case '\0': break;
			case '\n':
			case '\r':
				if (cstr.GetLength() > 0)
				{
					if (columns.empty())
					{
						getColumns(columns, cstr);
						cstr = _T("");
						continue;
					}

					for (col = 0, ofs = 0, vsi = columns.begin(); vsi != columns.end(); ++vsi)
					{
						ctmp = cstr.Mid(ofs,vsi->GetLength());
						ctmp = ctmp.Trim();
						vcol[col++].push_back(ctmp);
						ofs += vsi->GetLength();
					}
					cstr = _T("");
					++rows;
				}
				break;
			default:
				cstr += (char)ch;
			}
		}
		_pclose(pPipe);

		for (i = 0; i < rows; ++i)
		{
			vsj = vcol[0].begin();
			if (vsj == vcol[0].end())
				break;
			nIndex = m_WSLlistCtrl.InsertItem(i, *vsj);
			vcol[0].erase(vsj);
			for (col = 1, vsi = columns.begin()+1; vsi != columns.end(); ++vsi)
			{
				vsj = vcol[col].begin();
				if (vsj == vcol[col].end())
					continue;
				m_WSLlistCtrl.SetItemText(nIndex, col, *vsj);
				vcol[col].erase(vsj);
				++col;
			}
		}
	}

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
	// TODO: Add your control notification handler code here
	int selectedItem = pNMLV->iItem;
	CString msg;
	//msg.Format(_T("Item # %d"), selectedItem);
	msg = m_WSLlistCtrl.GetItemText(selectedItem, 1);
	*pResult = 0;
	CWnd* label = GetDlgItem(IDC_STATIC2);
	label->SetWindowTextW(msg);
//	AfxMessageBox(_T("CLICK"));
}

void CWSLTuxDlg::AddIconToSysTray()
{
	HICON	m_hIconInfo = NULL;//(HICON)::LoadImage(), MAKEINTRESOURCE(IDI_ICON_INFO), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_LOADTRANSPARENT);

	// TODO: Add your control notification handler code here
	NOTIFYICONDATA NID;

	memset(&NID, 0, sizeof(NID));

	//on main function:
	NID.cbSize = sizeof(NID);
	NID.hIcon = this->m_hIcon;

	NID.hWnd = this->m_hWnd;
	NID.uID = WM_TRAY_ID;
	StrCpyW(NID.szTip, L"WSL Tux");
	NID.uCallbackMessage = WM_TRAY_MESSAGE;
	//in a timer:

	NID.uFlags = NID.uFlags | NIF_ICON | NIF_TIP | NIF_MESSAGE;
	Shell_NotifyIcon(NIM_ADD, &NID);

	//CDialogEx::OnOK();
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


void CWSLTuxDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here

	this->ShowWindow(SW_HIDE);
//	CDialogEx::OnOK();
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
	CString dbgmsg;

	dbgmsg.Format(_T("OnTrayNotify(%d, %d)\n"), (UINT)wParam, (UINT)lParam);
	OutputDebugString(dbgmsg);

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
