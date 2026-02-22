#include "pch.h"
#include "framework.h"
#include "Resource.h"
#include "Page1.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CPage1::CPage1(CWnd* pParent)
	: CDialogEx(IDD_PAGE1, pParent)
	, m_bTimerRunning(FALSE)
{
}

void CPage1::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PAGE1_TIME, m_staticTime);
}

BEGIN_MESSAGE_MAP(CPage1, CDialogEx)
	ON_BN_CLICKED(IDC_PAGE1_BTN_START, &CPage1::OnBnClickedStart)
	ON_BN_CLICKED(IDC_PAGE1_BTN_STOP, &CPage1::OnBnClickedStop)
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CPage1::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 현재 시간 표시
	CTime time = CTime::GetCurrentTime();
	CString strTime = time.Format(_T("%H:%M:%S"));
	m_staticTime.SetWindowText(strTime);

	return TRUE;
}

void CPage1::OnBnClickedStart()
{
	if (!m_bTimerRunning)
	{
		SetTimer(IDT_PAGE1_TIMER, 1000, nullptr);
		m_bTimerRunning = TRUE;
	}
}

void CPage1::OnBnClickedStop()
{
	if (m_bTimerRunning)
	{
		KillTimer(IDT_PAGE1_TIMER);
		m_bTimerRunning = FALSE;
	}
}

void CPage1::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IDT_PAGE1_TIMER)
	{
		CTime time = CTime::GetCurrentTime();
		CString strTime = time.Format(_T("%H:%M:%S"));
		m_staticTime.SetWindowText(strTime);
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CPage1::OnDestroy()
{
	if (m_bTimerRunning)
	{
		KillTimer(IDT_PAGE1_TIMER);
		m_bTimerRunning = FALSE;
	}

	CDialogEx::OnDestroy();
}
