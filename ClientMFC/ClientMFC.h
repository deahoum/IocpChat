
// ClientMFC.h : PROJECT_NAME ���� ���α׷��� ���� �� ��� �����Դϴ�.
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH�� ���� �� ������ �����ϱ� ���� 'stdafx.h'�� �����մϴ�."
#endif

#include "resource.h"		// �� ��ȣ�Դϴ�.


// CClientMFCApp:
// �� Ŭ������ ������ ���ؼ��� ClientMFC.cpp�� �����Ͻʽÿ�.
//

class CClientMFCApp : public CWinApp
{
public:
	CClientMFCApp();

// �������Դϴ�.
public:
	virtual BOOL InitInstance();

// �����Դϴ�.

	DECLARE_MESSAGE_MAP()
};

extern CClientMFCApp theApp;