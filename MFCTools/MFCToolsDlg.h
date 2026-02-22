
// MFCToolsDlg.h: 헤더 파일
//

#pragma once

#include "Page1.h"
#include "Page2.h"
#include "Page3.h"
#include "Page4.h"
#include "Page5.h"

#define PAGE_COUNT 5

// CMFCToolsDlg 대화 상자
class CMFCToolsDlg : public CDialogEx
{
// 생성입니다.
public:
	CMFCToolsDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFCTOOLS_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;
	CTabCtrl m_tabCtrl;

	CPage1 m_page1;
	CPage2 m_page2;
	CPage3 m_page3;
	CPage4 m_page4;
	CPage5 m_page5;

	CDialogEx* m_pPages[PAGE_COUNT];
	int m_nCurrentPage;

	void ShowPage(int nPage);

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTcnSelChangeTab(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()
};
