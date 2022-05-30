
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

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_WSLlistCtrl;
	afx_msg void OnLvnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult);
	bool GetWSLInfo();
	void AddIconToSysTray();
	void RemIconFromSysTray();
	void Shutdown();
	afx_msg void OnBnClickedOk();
	afx_msg LRESULT OnTrayNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedCancel();
	afx_msg void OnClose();
};
