// QuickPlayerDlg.h : header file
//

#pragma once


// CQuickPlayerDlg dialog
class CQuickPlayerDlg : public CDialog
{
// Construction
public:
	CQuickPlayerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_QUICKPLAYER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedButton3();
    afx_msg void OnBnClickedRadio1();
    afx_msg void OnBnClickedRadio2();

    int getSIDMode(int combo);
    afx_msg void OnBnClickedBtnsid1();
    afx_msg void OnBnClickedBtnay1();
};
