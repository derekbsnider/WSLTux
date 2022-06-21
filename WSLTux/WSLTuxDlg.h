
// WSLTuxDlg.h : header file
//

#pragma once

#include <vector>
#include <string>

#define UWM_TRAY_ID			(WM_USER + 0x2)
#define UWM_TRAY_MESSAGE	(WM_USER + 0x100)
#define IDT_MINUTE_TIMER	(WM_USER + 0x200)
#define IDT_CLICK_TIMER		(WM_USER + 0x201)

#define TIMER_INTERVAL 2000
#define CLICK_INTERVAL 250
#define MAX_KEY_LENGTH 256
#define MAX_VALUE_NAME 16383

class wslDistribution
{
public:
	CString regkey;
	CString name;
	CString packagefamilyname;
	CString basepath;
	std::wstring wsname;
	DWORD version;
	DWORD state;
	DWORD flags;
	DWORD uid;
	bool running;
	std::vector<DWORD> pids;
	void clear() { pids.clear(); }
	wslDistribution()
	{
		version = 0;
		state = 0;
		flags = 0;
		uid = 0;
		running = false;
	}
	wslDistribution(const wslDistribution& d)
		: regkey(d.regkey), name(d.name), packagefamilyname(d.packagefamilyname),
		  basepath(d.basepath), version(d.version), state(d.state),
		  flags(d.flags), uid(d.uid), running(d.running)
	{
		wsname = name;
	}
	wslDistribution(CString k, CString n, DWORD v) : regkey(k), name(n), version(v)
	{
		state = 0;
		flags = 0;
		uid = 0;
		running = false;
		wsname = name;
	}
};

class WSLInfo
{
protected:
	HANDLE wiMutex;
	void init();
public:
	int rows;
	size_t dists_running;
	size_t procs_running;
	std::vector<CString> columns;
	std::vector<CString> vcol[10];
	std::vector<wslDistribution> distributions;
	bool lock();
	bool unlock();
	void clear();
	WSLInfo() { init(); }
    ~WSLInfo();
};


// CWSLTuxDlg dialog
class CWSLTuxDlg : public CDialogEx
{
// Construction
public:
	CWSLTuxDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_WSLTUX_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
	HICON m_hIcon;
	HICON m_hIcon_disabled;
	WSLInfo wslinfo;
	CString m_cmdline;
	bool m_visible;
	bool m_initialized = false;
	HRESULT RunExternalProgram(CString cmd);
	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	UINT_PTR m_minuteTimer = NULL;
	UINT_PTR m_clickTimer = NULL;
	CListCtrl m_WSLlistCtrl;
	CString ProgramOutput;
	CString lsxxDefaultDistribution;
	CString DefaultDistribution;
	DWORD 	lsxxDefaultVersion = 2;
	DWORD	m_clicks = 0;
	HANDLE m_hChildStd_OUT_Rd = 0;
	HANDLE m_hChildStd_OUT_Wr = 0;
	HANDLE m_hreadDataFromExtProgram = 0;

	bool GetWSLInfo();
	void RefreshWSLInfo();
	void PopulateWSLlist();
	void AddIconToSysTray();
	void UpdateSysTrayIcon();
	void RemIconFromSysTray();
	void Shutdown();
	void StopTimer();
	void StartTimer(UINT interval=TIMER_INTERVAL);
	void RestartTimer();
	void StartClickTimer();
	void StopClickTimer();
	bool GetDistributionList();
	bool GetDistributionStates();
	bool GetDistributionProcs();
	void ShowMyWindow();
	void HideMyWindow();
	CString WSLstopDistribution(CString& distro);
	CString WSLstartDistribution(CString& distro);
	static BOOL CALLBACK searcher(HWND hWnd, LPARAM lParam);

	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnClose();
	afx_msg void OnBnClickedStop();
	afx_msg void OnBnClickedStart();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnLvnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
	afx_msg LRESULT OnTrayNotify(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnAreYouMe(WPARAM, LPARAM);
	afx_msg void OnEndSession(BOOL bEnding);
};
