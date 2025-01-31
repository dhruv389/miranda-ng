/*

WhatsApp plugin for Miranda NG
Copyright � 2019-22 George Hazan

*/

#include "stdafx.h"

/////////////////////////////////////////////////////////////////////////////////////////

class COptionsDlg : public CProtoDlgBase<WhatsAppProto>
{
	CCtrlCheck chkHideChats, chkBbcodes;
	CCtrlEdit edtGroup, edtNick, edtDevName;
	ptrW m_wszOldGroup;

public:
	COptionsDlg(WhatsAppProto *ppro, int iDlgID, bool bFullDlg) :
		CProtoDlgBase<WhatsAppProto>(ppro, iDlgID),
		chkBbcodes(this, IDC_USEBBCODES),
		chkHideChats(this, IDC_HIDECHATS),
		edtNick(this, IDC_NICK),
		edtGroup(this, IDC_DEFGROUP),
		edtDevName(this, IDC_DEVICE_NAME),
		m_wszOldGroup(mir_wstrdup(ppro->m_wszDefaultGroup))
	{
		CreateLink(edtNick, ppro->m_wszNick);
		CreateLink(edtGroup, ppro->m_wszDefaultGroup);
		CreateLink(edtDevName, ppro->m_wszDeviceName);

		if (bFullDlg) {
			CreateLink(chkHideChats, ppro->m_bHideGroupchats);
			CreateLink(chkBbcodes, ppro->m_bUseBbcodes);
		}
	}

	bool OnInitDialog() override
	{
		if (!m_proto->getMStringA(DBKEY_ID).IsEmpty())
			edtDevName.Disable();
		return true;
	}

	bool OnApply() override
	{
		if (mir_wstrlen(m_proto->m_wszNick)) {
			SetFocus(edtNick.GetHwnd());
			return false;
		}

		if (mir_wstrcmp(m_proto->m_wszDefaultGroup, m_wszOldGroup))
			Clist_GroupCreate(0, m_proto->m_wszDefaultGroup);
		return true;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR WhatsAppProto::SvcCreateAccMgrUI(WPARAM, LPARAM hwndParent)
{
	auto *pDlg = new COptionsDlg(this, IDD_ACCMGRUI, false);
	pDlg->SetParent((HWND)hwndParent);
	pDlg->Create();
	return (INT_PTR)pDlg->GetHwnd();
}

int WhatsAppProto::OnOptionsInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.szTitle.w = m_tszUserName;
	odp.flags = ODPF_UNICODE;
	odp.szGroup.w = LPGENW("Network");

	odp.position = 1;
	odp.szTab.w = LPGENW("Account");
	odp.pDialog = new COptionsDlg(this, IDD_OPTIONS, true);
	g_plugin.addOptions(wParam, &odp);
	return 0;
}
