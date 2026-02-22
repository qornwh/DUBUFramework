#include "pch.h"
#include "framework.h"
#include "Resource.h"
#include "Page2.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CPage2::CPage2(CWnd* pParent)
	: CDialogEx(IDD_PAGE2, pParent)
{
}

void CPage2::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CPage2, CDialogEx)
END_MESSAGE_MAP()
