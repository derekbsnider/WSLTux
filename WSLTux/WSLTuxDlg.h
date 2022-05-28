
// WSLTuxDlg.h : header file
//

#pragma once

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

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_WSLlistCtrl;
	afx_msg void OnLvnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult);
	void AddIconToSysTray();
	void RemIconFromSysTray();
	void Shutdown();
	afx_msg void OnBnClickedOk();
	afx_msg LRESULT OnTrayNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedCancel();
	afx_msg void OnClose();
};
