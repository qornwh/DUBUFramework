#include "pch.h"
#include "framework.h"
#include "Resource.h"
#include "Page3.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CPage3::CPage3(CWnd* pParent)
	: CDialogEx(IDD_PAGE3, pParent)
{
}

void CPage3::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CPage3, CDialogEx)
END_MESSAGE_MAP()
