
// WSLTuxDlg.h : header file
//

#pragma once

#include <vector>

class WSLInfo
{
public:
	int rows;
	int num_running;
	std::vector<CString> columns;
	std::vector<CString> vcol[10];
	void clear();
	WSLInfo() { clear(); }
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

	HRESULT RunExternalProgram(CString cmd);
	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	UINT_PTR m_minuteTimer = NULL;
	CListCtrl m_WSLlistCtrl;
	CString ProgramOutput;
	HANDLE m_hChildStd_OUT_Rd = 0;
	HANDLE m_hChildStd_OUT_Wr = 0;
	HANDLE m_hreadDataFromExtProgram = 0;

	afx_msg void OnLvnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult);
	bool GetWSLInfo();
	void RefreshWSLInfo();
	void PopulateWSLlist();
	void AddIconToSysTray();
	void UpdateSysTrayIcon();
	void RemIconFromSysTray();
	void Shutdown();
	CString WSLstopDistribution(CString& distro);
	CString WSLstartDistribution(CString& distro);

	afx_msg void OnBnClickedOk();
	afx_msg LRESULT OnTrayNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedCancel();
	afx_msg void OnClose();
	afx_msg void OnBnClickedStop();
	afx_msg void OnBnClickedStart();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
