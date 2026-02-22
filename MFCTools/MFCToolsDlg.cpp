
// MFCToolsDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "MFCTools.h"
#include "MFCToolsDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CMFCToolsDlg 대화 상자



CMFCToolsDlg::CMFCToolsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFCTOOLS_DIALOG, pParent)
	, m_nCurrentPage(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFCToolsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_MAIN, m_tabCtrl);
}

BEGIN_MESSAGE_MAP(CMFCToolsDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAIN, &CMFCToolsDlg::OnTcnSelChangeTab)
END_MESSAGE_MAP()


// CMFCToolsDlg 메시지 처리기

BOOL CMFCToolsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
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

	// 이 대화 상자의 아이콘을 설정합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// 탭 추가
	for (int i = 0; i < PAGE_COUNT; i++)
	{
		m_tabCtrl.InsertItem(i, _T("NONE"));
	}

	// 페이지 생성
	m_page1.Create(IDD_PAGE1, &m_tabCtrl);
	m_page2.Create(IDD_PAGE2, &m_tabCtrl);
	m_page3.Create(IDD_PAGE3, &m_tabCtrl);
	m_page4.Create(IDD_PAGE4, &m_tabCtrl);
	m_page5.Create(IDD_PAGE5, &m_tabCtrl);

	m_pPages[0] = &m_page1;
	m_pPages[1] = &m_page2;
	m_pPages[2] = &m_page3;
	m_pPages[3] = &m_page4;
	m_pPages[4] = &m_page5;

	// 탭 내부 영역 계산 후 페이지 위치 설정
	CRect tabRect;
	m_tabCtrl.GetClientRect(&tabRect);
	m_tabCtrl.AdjustRect(FALSE, &tabRect);

	for (int i = 0; i < PAGE_COUNT; i++)
	{
		m_pPages[i]->MoveWindow(&tabRect);
	}

	// 첫 번째 페이지 표시
	ShowPage(0);

	return TRUE;
}

void CMFCToolsDlg::ShowPage(int nPage)
{
	if (nPage < 0 || nPage >= PAGE_COUNT)
		return;

	for (int i = 0; i < PAGE_COUNT; i++)
	{
		if (i == nPage)
			m_pPages[i]->ShowWindow(SW_SHOW);
		else
			m_pPages[i]->ShowWindow(SW_HIDE);
	}

	m_nCurrentPage = nPage;
}

void CMFCToolsDlg::OnTcnSelChangeTab(NMHDR* pNMHDR, LRESULT* pResult)
{
	int nSel = m_tabCtrl.GetCurSel();
	ShowPage(nSel);
	*pResult = 0;
}

void CMFCToolsDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CMFCToolsDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CMFCToolsDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}
