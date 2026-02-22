#pragma once

class CPage1 : public CDialogEx
{
public:
	CPage1(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PAGE1 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedStart();
	afx_msg void OnBnClickedStop();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()

private:
	CStatic m_staticTime;
	BOOL m_bTimerRunning;
};
