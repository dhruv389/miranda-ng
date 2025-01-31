/*
	New Away System - plugin for Miranda IM
	Copyright (C) 2005-2007 Chervov Dmitry

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "stdafx.h"
#include "MsgTree.h"
#include "Properties.h"
#include "Path.h"
#include "m_button.h"
#include "m_clc.h"
#include "GroupCheckbox.h"

int g_Messages_RecentRootID, g_Messages_PredefinedRootID;

// Set window size and center its controls
void MySetPos(HWND hwndParent)
{
	HWND hWndTab = FindWindowEx(GetParent(hwndParent), nullptr, L"SysTabControl32", L"");
	if (!hWndTab) {
		_ASSERT(0);
		return;
	}
	RECT rcDlg;
	GetClientRect(hWndTab, &rcDlg);
	TabCtrl_AdjustRect(hWndTab, false, &rcDlg);
	rcDlg.right -= rcDlg.left - 2;
	rcDlg.bottom -= rcDlg.top;
	rcDlg.top += 2;
	if (hwndParent) {
		RECT OldSize;
		GetClientRect(hwndParent, &OldSize);
		MoveWindow(hwndParent, rcDlg.left, rcDlg.top, rcDlg.right, rcDlg.bottom, true);
		int dx = (rcDlg.right - OldSize.right) >> 1;
		int dy = (rcDlg.bottom - OldSize.bottom) >> 1;
		HWND hCurWnd = GetWindow(hwndParent, GW_CHILD);
		while (hCurWnd) {
			RECT CWOldPos;
			GetWindowRect(hCurWnd, &CWOldPos);
			POINT pt;
			pt.x = CWOldPos.left;
			pt.y = CWOldPos.top;
			ScreenToClient(hwndParent, &pt);
			SetWindowPos(hCurWnd, nullptr, pt.x + dx, pt.y + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			hCurWnd = GetNextWindow(hCurWnd, GW_HWNDNEXT);
		}
	}
}

// ================================================ Message options ================================================

COptPage g_MessagesOptPage(MODULENAME, nullptr);

void EnableMessagesOptDlgControls(CMsgTree* MsgTree)
{
	bool bIsNotGroup = false;
	bool bSelected = false;
	CBaseTreeItem *TreeItem = MsgTree->GetSelection();
	if (TreeItem && !(TreeItem->Flags & TIF_ROOTITEM)) {
		bIsNotGroup = !(TreeItem->Flags & TIF_GROUP);
		bSelected = true;
	}
	g_MessagesOptPage.Enable(IDC_MESSAGEDLG_DEL, bSelected);
	g_MessagesOptPage.Enable(IDC_MESSAGEDLG_MSGTITLE, bSelected);
	for (int i = 0; i < g_MessagesOptPage.Items.GetSize(); i++)
		if (g_MessagesOptPage.Items[i]->GetParam() == IDC_MESSAGEDLG_MSGTREE)
			g_MessagesOptPage.Items[i]->Enable(bIsNotGroup);

	SendDlgItemMessage(g_MessagesOptPage.GetWnd(), IDC_MESSAGEDLG_MSGDATA, EM_SETREADONLY, !bIsNotGroup, 0);
	g_MessagesOptPage.MemToPage(true);
}

static WNDPROC g_OrigDefStatusButtonMsgProc;

static LRESULT CALLBACK DefStatusButtonSubclassProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_LBUTTONUP && IsDlgButtonChecked(GetParent(hWnd), GetDlgCtrlID(hWnd)))
		return 0;

	return CallWindowProc(g_OrigDefStatusButtonMsgProc, hWnd, Msg, wParam, lParam);
}

struct {
	int DlgItem, Status;
}
static Dlg1DefMsgDlgItems[] = {
	{ IDC_MESSAGEDLG_DEF_ONL,  ID_STATUS_ONLINE    },
	{ IDC_MESSAGEDLG_DEF_AWAY, ID_STATUS_AWAY      },
	{ IDC_MESSAGEDLG_DEF_NA,   ID_STATUS_NA        },
	{ IDC_MESSAGEDLG_DEF_OCC,  ID_STATUS_OCCUPIED  },
	{ IDC_MESSAGEDLG_DEF_DND,  ID_STATUS_DND       },
	{ IDC_MESSAGEDLG_DEF_FFC,  ID_STATUS_FREECHAT  },
	{ IDC_MESSAGEDLG_DEF_INV,  ID_STATUS_INVISIBLE },
};

struct {
	int DlgItem, IconIndex;
	wchar_t* Text;
}
static Dlg1Buttons[] = {
	IDC_MESSAGEDLG_NEWMSG, IDI_NEWMESSAGE, LPGENW("Create new message"),
	IDC_MESSAGEDLG_NEWCAT, IDI_NEWCATEGORY, LPGENW("Create new category"),
	IDC_MESSAGEDLG_DEL, IDI_DELETE, LPGENW("Delete"),
	IDC_MESSAGEDLG_VARS, -1, LPGENW("Open Variables help dialog"),
};

static INT_PTR CALLBACK MessagesOptDlg(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int ChangeLock = 0;
	static CMsgTree* MsgTree = nullptr;
	
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		MySetPos(hwndDlg);
		ChangeLock++;
		g_MessagesOptPage.SetWnd(hwndDlg);
		SendDlgItemMessage(hwndDlg, IDC_MESSAGEDLG_MSGTITLE, EM_LIMITTEXT, TREEITEMTITLE_MAXLEN, 0);
		SendDlgItemMessage(hwndDlg, IDC_MESSAGEDLG_MSGDATA, EM_LIMITTEXT, AWAY_MSGDATA_MAX, 0);
		// init image buttons
		for (auto &it: Dlg1Buttons) {
			HWND hButton = GetDlgItem(hwndDlg, it.DlgItem);
			SendMessage(hButton, BUTTONADDTOOLTIP, (WPARAM)TranslateW(it.Text), BATF_UNICODE);
			SendMessage(hButton, BUTTONSETASFLATBTN, TRUE, 0);
		}
		// now default status message buttons
		for (auto &it: Dlg1DefMsgDlgItems) {
			HWND hButton = GetDlgItem(hwndDlg, it.DlgItem);
			SendMessage(hButton, BUTTONADDTOOLTIP, (WPARAM)Clist_GetStatusModeDescription(it.Status, 0), BATF_UNICODE);
			SendMessage(hButton, BUTTONSETASPUSHBTN, TRUE, 0);
			SendMessage(hButton, BUTTONSETASFLATBTN, TRUE, 0);
			g_OrigDefStatusButtonMsgProc = (WNDPROC)SetWindowLongPtr(hButton, GWLP_WNDPROC, (LONG_PTR)DefStatusButtonSubclassProc);
		}
		SendMessage(hwndDlg, UM_ICONSCHANGED, 0, 0);
		g_MessagesOptPage.DBToMemToPage();
		_ASSERT(!MsgTree);
		MsgTree = new CMsgTree(GetDlgItem(hwndDlg, IDC_MESSAGEDLG_MSGTREE));
		if (!MsgTree->SetSelection(MsgTree->GetDefMsg(ID_STATUS_AWAY), MTSS_BYID))
			MsgTree->SetSelection(g_Messages_PredefinedRootID, MTSS_BYID);

		ChangeLock--;
		return true;

	case UM_ICONSCHANGED:
		for (auto &it : Dlg1DefMsgDlgItems)
			SendDlgItemMessage(hwndDlg, it.DlgItem, BM_SETIMAGE, IMAGE_ICON, (LPARAM)Skin_LoadProtoIcon(nullptr, it.Status));

		for (auto &it : Dlg1Buttons)
			if (it.IconIndex != -1)
				SendDlgItemMessage(hwndDlg, it.DlgItem, BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_plugin.getIcon(it.IconIndex));

		variables_skin_helpbutton(hwndDlg, IDC_MESSAGEDLG_VARS);
		break;
	
	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == PSN_APPLY) {
			HWND hTreeView = GetDlgItem(hwndDlg, IDC_MESSAGEDLG_MSGTREE);
			HTREEITEM hSelectedItem = TreeView_GetSelection(hTreeView);
			ChangeLock++;
			TreeView_SelectItem(hTreeView, NULL);
			TreeView_SelectItem(hTreeView, hSelectedItem);
			ChangeLock--;
			MsgTree->Save();
			return true;
		}

		if (((LPNMHDR)lParam)->idFrom == IDC_MESSAGEDLG_MSGTREE) {
			PNMMSGTREE pnm = (PNMMSGTREE)lParam;
			switch (pnm->hdr.code) {
			case MTN_SELCHANGED:
				if (pnm->ItemOld && !(pnm->ItemOld->Flags & (TIF_ROOTITEM | TIF_GROUP))) {
					TCString Msg;
					GetDlgItemText(hwndDlg, IDC_MESSAGEDLG_MSGDATA, Msg.GetBuffer(AWAY_MSGDATA_MAX), AWAY_MSGDATA_MAX);
					Msg.ReleaseBuffer();
					if (((CTreeItem*)pnm->ItemOld)->User_Str1 != (const wchar_t*)Msg) {
						((CTreeItem*)pnm->ItemOld)->User_Str1 = Msg;
						MsgTree->SetModified(true);
					}
				}
				if (pnm->ItemNew) {
					ChangeLock++;
					if (!(pnm->ItemNew->Flags & TIF_ROOTITEM)) {
						SetDlgItemText(hwndDlg, IDC_MESSAGEDLG_MSGTITLE, pnm->ItemNew->Title);
						SetDlgItemText(hwndDlg, IDC_MESSAGEDLG_MSGDATA, (pnm->ItemNew->Flags & TIF_GROUP) ? L"" : ((CTreeItem*)pnm->ItemNew)->User_Str1);
					}
					else {
						SetDlgItemText(hwndDlg, IDC_MESSAGEDLG_MSGTITLE, L"");
						if (pnm->ItemNew->ID == g_Messages_RecentRootID)
							SetDlgItemText(hwndDlg, IDC_MESSAGEDLG_MSGDATA, TranslateT("Your most recent status messages are placed in this category. It's not recommended that you put your messages manually here, as they'll be replaced by your recent messages."));
						else {
							_ASSERT(pnm->ItemNew->ID == g_Messages_PredefinedRootID);
							SetDlgItemText(hwndDlg, IDC_MESSAGEDLG_MSGDATA, TranslateT("You can put your frequently used and favorite messages in this category."));
						}
					}
					for (auto &it: Dlg1DefMsgDlgItems) {
						COptItem_Checkbox *Checkbox = (COptItem_Checkbox*)g_MessagesOptPage.Find(it.DlgItem);
						Checkbox->SetWndValue(g_MessagesOptPage.GetWnd(), MsgTree->GetDefMsg(it.Status) == pnm->ItemNew->ID);
					}
					ChangeLock--;
				}
				EnableMessagesOptDlgControls(MsgTree);
				return 0;

			case MTN_DEFMSGCHANGED:
				if (!ChangeLock) {
					CBaseTreeItem *SelectedItem = MsgTree->GetSelection();
					_ASSERT(SelectedItem);
					// SelectedItem contains the same info as one of ItemOld or ItemNew - so we'll just use SelectedItem and won't bother with identifying which of ItemOld or ItemNew is currently selected
					if ((pnm->ItemOld && pnm->ItemOld->ID == SelectedItem->ID) || (pnm->ItemNew && pnm->ItemNew->ID == SelectedItem->ID)) {
						for (auto &it: Dlg1DefMsgDlgItems) {
							COptItem_Checkbox *Checkbox = (COptItem_Checkbox*)g_MessagesOptPage.Find(it.DlgItem);
							Checkbox->SetWndValue(g_MessagesOptPage.GetWnd(), MsgTree->GetDefMsg(it.Status) == SelectedItem->ID);
						}
					}
				}
				if (!ChangeLock)
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, 0);
				return 0;

			case MTN_ITEMRENAMED:
				{
					CBaseTreeItem *SelectedItem = MsgTree->GetSelection();
					_ASSERT(SelectedItem);
					if (pnm->ItemNew->ID == SelectedItem->ID && !ChangeLock) {
						ChangeLock++;
						SetDlgItemText(hwndDlg, IDC_MESSAGEDLG_MSGTITLE, pnm->ItemNew->Title);
						ChangeLock--;
					}
				} // go through
			case MTN_ENDDRAG:
			case MTN_NEWCATEGORY:
			case MTN_NEWMESSAGE:
			case MTN_DELETEITEM:
			case TVN_ITEMEXPANDED:
				if (!ChangeLock)
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, 0);
				return 0;
			}
		}
		break;

	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDC_MESSAGEDLG_DEF_ONL:
			case IDC_MESSAGEDLG_DEF_AWAY:
			case IDC_MESSAGEDLG_DEF_NA:
			case IDC_MESSAGEDLG_DEF_OCC:
			case IDC_MESSAGEDLG_DEF_DND:
			case IDC_MESSAGEDLG_DEF_FFC:
			case IDC_MESSAGEDLG_DEF_INV:
				for (auto &it: Dlg1DefMsgDlgItems) {
					if (LOWORD(wParam) == it.DlgItem) {
						MsgTree->SetDefMsg(it.Status, MsgTree->GetSelection()->ID); // PSM_CHANGED is sent here through MTN_DEFMSGCHANGED, so we don't need to send it once more
						break;
					}
				}
				break;
			case IDC_MESSAGEDLG_VARS:
				my_variables_showhelp(hwndDlg, IDC_MESSAGEDLG_MSGDATA);
				break;
			case IDC_MESSAGEDLG_DEL:
				MsgTree->EnsureVisible(MsgTree->GetSelection()->hItem);
				MsgTree->DeleteSelectedItem();
				break;
			case IDC_MESSAGEDLG_NEWCAT:
				MsgTree->AddCategory();
				SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGEDLG_MSGTITLE));
				break;
			case IDC_MESSAGEDLG_NEWMSG:
				MsgTree->AddMessage();
				SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGEDLG_MSGTITLE));
				break;
			}
			break;

		case EN_CHANGE:
			if (LOWORD(wParam) == IDC_MESSAGEDLG_MSGDATA || LOWORD(wParam) == IDC_MESSAGEDLG_MSGTITLE) {
				if (!ChangeLock) {
					if (LOWORD(wParam) == IDC_MESSAGEDLG_MSGTITLE) {
						CBaseTreeItem* TreeItem = MsgTree->GetSelection();
						if (TreeItem && !(TreeItem->Flags & TIF_ROOTITEM)) {
							GetDlgItemText(hwndDlg, IDC_MESSAGEDLG_MSGTITLE, TreeItem->Title.GetBuffer(TREEITEMTITLE_MAXLEN), TREEITEMTITLE_MAXLEN);
							TreeItem->Title.ReleaseBuffer();
							ChangeLock++;
							MsgTree->UpdateItem(TreeItem->ID);
							ChangeLock--;
						}
					}
					MsgTree->SetModified(true);
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, 0);
				}
			}
			break;
		}
		break;

	case WM_DESTROY:
		delete MsgTree;
		MsgTree = nullptr;
		g_MessagesOptPage.SetWnd(nullptr);
		break;
	}
	return 0;
}

// ================================================ Main options ================================================

COptPage g_MoreOptPage(MODULENAME, nullptr);

void EnableMoreOptDlgControls()
{
	g_MoreOptPage.Enable(IDC_MOREOPTDLG_PERSTATUSPERSONAL, g_MoreOptPage.GetWndValue(IDC_MOREOPTDLG_SAVEPERSONALMSGS) != 0);
	bool bEnabled = g_MoreOptPage.GetWndValue(IDC_MOREOPTDLG_RECENTMSGSCOUNT) != 0;
	g_MoreOptPage.Enable(IDC_MOREOPTDLG_PERSTATUSMRM, bEnabled);
	g_MoreOptPage.Enable(IDC_MOREOPTDLG_USELASTMSG, bEnabled);
	g_MoreOptPage.Enable(IDC_MOREOPTDLG_USEDEFMSG, bEnabled);
	g_MoreOptPage.Enable(IDC_MOREOPTDLG_PERSTATUSPERSONAL, g_MoreOptPage.GetWndValue(IDC_MOREOPTDLG_SAVEPERSONALMSGS) != 0);
	g_MoreOptPage.Enable(IDC_MOREOPTDLG_UPDATEMSGSPERIOD, g_MoreOptPage.GetWndValue(IDC_MOREOPTDLG_UPDATEMSGS) != 0);
	InvalidateRect(GetDlgItem(g_MoreOptPage.GetWnd(), IDC_MOREOPTDLG_UPDATEMSGSPERIOD_SPIN), nullptr, false); // update spin control
	g_MoreOptPage.MemToPage(true);
}

struct {
	int DlgItem, Status;
}
static Dlg2StatusButtons[] = {
	{ IDC_MOREOPTDLG_DONTPOPDLG_ONL,  ID_STATUS_ONLINE    },
	{ IDC_MOREOPTDLG_DONTPOPDLG_AWAY, ID_STATUS_AWAY      },
	{ IDC_MOREOPTDLG_DONTPOPDLG_NA,   ID_STATUS_NA        },
	{ IDC_MOREOPTDLG_DONTPOPDLG_OCC,  ID_STATUS_OCCUPIED  },
	{ IDC_MOREOPTDLG_DONTPOPDLG_DND,  ID_STATUS_DND       },
	{ IDC_MOREOPTDLG_DONTPOPDLG_FFC,  ID_STATUS_FREECHAT  },
	{ IDC_MOREOPTDLG_DONTPOPDLG_INV,  ID_STATUS_INVISIBLE },
};

static INT_PTR CALLBACK MoreOptDlg(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int ChangeLock = 0;
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		MySetPos(hwndDlg);
		ChangeLock++;
		g_MoreOptPage.SetWnd(hwndDlg);
		SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_WAITFORMSG, EM_LIMITTEXT, 4, 0);
		SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_RECENTMSGSCOUNT, EM_LIMITTEXT, 2, 0);
		SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_WAITFORMSG_SPIN, UDM_SETRANGE32, -1, 9999);
		SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_RECENTMSGSCOUNT_SPIN, UDM_SETRANGE32, 0, 99);
		SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_UPDATEMSGSPERIOD_SPIN, UDM_SETRANGE32, 30, 99999);
		for (auto &it: Dlg2StatusButtons) {
			HWND hButton = GetDlgItem(hwndDlg, it.DlgItem);
			SendMessage(hButton, BUTTONADDTOOLTIP, (WPARAM)Clist_GetStatusModeDescription(it.Status, 0), BATF_UNICODE);
			SendMessage(hButton, BUTTONSETASPUSHBTN, TRUE, 0);
			SendMessage(hButton, BUTTONSETASFLATBTN, TRUE, 0);
		}
		SendMessage(hwndDlg, UM_ICONSCHANGED, 0, 0);
		g_MoreOptPage.DBToMemToPage();
		EnableMoreOptDlgControls();
		ChangeLock--;
		return true;

	case UM_ICONSCHANGED:
		for (auto &it: Dlg2StatusButtons)
			SendDlgItemMessage(hwndDlg, it.DlgItem, BM_SETIMAGE, IMAGE_ICON, (LPARAM)Skin_LoadProtoIcon(nullptr, it.Status));
		break;

	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == PSN_APPLY) {
			g_MoreOptPage.PageToMemToDB();
			InitUpdateMsgs();
			return true;
		}
		break;

	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {

			case IDC_MOREOPTDLG_RESETPROTOMSGS:
			case IDC_MOREOPTDLG_SAVEPERSONALMSGS:
			case IDC_MOREOPTDLG_UPDATEMSGS:
				EnableMoreOptDlgControls();
				// go through
			case IDC_MOREOPTDLG_PERSTATUSMRM:
			case IDC_MOREOPTDLG_PERSTATUSPROTOMSGS:
			case IDC_MOREOPTDLG_PERSTATUSPROTOSETTINGS:
			case IDC_MOREOPTDLG_PERSTATUSPERSONAL:
			case IDC_MOREOPTDLG_PERSTATUSPERSONALSETTINGS:
			case IDC_MOREOPTDLG_MYNICKPERPROTO:
			case IDC_MOREOPTDLG_USEDEFMSG:
			case IDC_MOREOPTDLG_USELASTMSG:
			case IDC_MOREOPTDLG_DONTPOPDLG_ONL:
			case IDC_MOREOPTDLG_DONTPOPDLG_AWAY:
			case IDC_MOREOPTDLG_DONTPOPDLG_NA:
			case IDC_MOREOPTDLG_DONTPOPDLG_OCC:
			case IDC_MOREOPTDLG_DONTPOPDLG_DND:
			case IDC_MOREOPTDLG_DONTPOPDLG_FFC:
			case IDC_MOREOPTDLG_DONTPOPDLG_INV:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, 0);
				return 0;
			}
			break;
		case EN_CHANGE:
			if (!ChangeLock && g_MoreOptPage.GetWnd()) {
				if (LOWORD(wParam) == IDC_MOREOPTDLG_RECENTMSGSCOUNT)
					EnableMoreOptDlgControls();

				SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, 0);
			}
		}
		break;

	case WM_DESTROY:
		g_MoreOptPage.SetWnd(nullptr);
		break;
	}
	return 0;
}

// ================================================ Autoreply options ================================================

COptPage g_AutoreplyOptPage(MODULENAME, nullptr);

void EnableAutoreplyOptDlgControls()
{
	g_AutoreplyOptPage.PageToMem();
	bool bAutoreply = g_AutoreplyOptPage.GetValue(IDC_REPLYDLG_ENABLEREPLY) != 0;

	for (int i = 0; i < g_AutoreplyOptPage.Items.GetSize(); i++) {
		switch (g_AutoreplyOptPage.Items[i]->GetParam()) {
		case IDC_REPLYDLG_ENABLEREPLY:
			g_AutoreplyOptPage.Items[i]->Enable(bAutoreply);
			break;
		case IDC_REPLYDLG_SENDCOUNT:
			g_AutoreplyOptPage.Items[i]->Enable(bAutoreply && g_AutoreplyOptPage.GetValue(IDC_REPLYDLG_SENDCOUNT) > 0);
			break;
		}
	}
	g_AutoreplyOptPage.MemToPage(true);
	InvalidateRect(GetDlgItem(g_AutoreplyOptPage.GetWnd(), IDC_REPLYDLG_SENDCOUNT_SPIN), nullptr, 0); // update spin control
}

static struct {
	int DlgItem, Status;
}
Dlg3StatusButtons[] = {
	{ IDC_REPLYDLG_DISABLE_ONL,  ID_STATUS_ONLINE    },
	{ IDC_REPLYDLG_DISABLE_AWAY, ID_STATUS_AWAY      },
	{ IDC_REPLYDLG_DISABLE_NA,   ID_STATUS_NA        },
	{ IDC_REPLYDLG_DISABLE_OCC,  ID_STATUS_OCCUPIED  },
	{ IDC_REPLYDLG_DISABLE_DND,  ID_STATUS_DND       },
	{ IDC_REPLYDLG_DISABLE_FFC,  ID_STATUS_FREECHAT  },
	{ IDC_REPLYDLG_DISABLE_INV,  ID_STATUS_INVISIBLE },
};

INT_PTR CALLBACK AutoreplyOptDlg(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int ChangeLock = 0;
	static HWND hWndTooltips;
	static HFONT s_hDrawFont;

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		{
			ChangeLock++;
			MySetPos(hwndDlg);
			g_AutoreplyOptPage.SetWnd(hwndDlg);

			LOGFONT logFont;
			HFONT hFont = (HFONT)SendDlgItemMessage(hwndDlg, IDC_REPLYDLG_STATIC_DISABLEWHENSTATUS, WM_GETFONT, 0, 0); // try getting the font used by the control
			if (!hFont)
				hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

			GetObject(hFont, sizeof(logFont), &logFont);
			logFont.lfWeight = FW_BOLD;
			s_hDrawFont = CreateFontIndirect(&logFont); // recreate the font
			SendDlgItemMessage(hwndDlg, IDC_REPLYDLG_STATIC_DISABLEWHENSTATUS, WM_SETFONT, (WPARAM)s_hDrawFont, true);
			SendDlgItemMessage(hwndDlg, IDC_REPLYDLG_STATIC_FORMAT, WM_SETFONT, (WPARAM)s_hDrawFont, true);

			SendDlgItemMessage(hwndDlg, IDC_REPLYDLG_SENDCOUNT, EM_LIMITTEXT, 3, 0);
			SendDlgItemMessage(hwndDlg, IDC_REPLYDLG_SENDCOUNT_SPIN, UDM_SETRANGE32, -1, 999);

			HWND hCombo = GetDlgItem(hwndDlg, IDC_REPLYDLG_ONLYIDLEREPLY_COMBO);
			struct {
				wchar_t *Text;
				int Meaning;
			}
			static IdleComboValues[] = {
				L"Windows", AUTOREPLY_IDLE_WINDOWS,
				L"Miranda", AUTOREPLY_IDLE_MIRANDA
			};

			for (auto &it: IdleComboValues)
				SendMessage(hCombo, CB_SETITEMDATA, SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TranslateW(it.Text)), it.Meaning);

			for (auto &it: Dlg3StatusButtons) {
				HWND hButton = GetDlgItem(hwndDlg, it.DlgItem);
				SendMessage(hButton, BUTTONADDTOOLTIP, (WPARAM)Clist_GetStatusModeDescription(it.Status, 0), BATF_UNICODE);
				SendMessage(hButton, BUTTONSETASPUSHBTN, TRUE, 0);
				SendMessage(hButton, BUTTONSETASFLATBTN, TRUE, 0);
			}
			HWND hButton = GetDlgItem(hwndDlg, IDC_REPLYDLG_VARS);
			SendMessage(hButton, BUTTONADDTOOLTIP, (WPARAM)TranslateT("Open Variables help dialog"), BATF_UNICODE);
			SendMessage(hButton, BUTTONSETASFLATBTN, TRUE, 0);

			SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_EVNTMSG, BUTTONSETASTHEMEDBTN, TRUE, 0);
			SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_EVNTFILE, BUTTONSETASTHEMEDBTN, TRUE, 0);
			SendMessage(hwndDlg, UM_ICONSCHANGED, 0, 0);

			// init tooltips
			struct {
				int m_dlgItemID;
				wchar_t *Text;
			}
			Tooltips[] = {
				IDC_REPLYDLG_RESETCOUNTERWHENSAMEICON, LPGENW("When this checkbox is ticked, NewAwaySys counts \"send times\" starting from the last status message change, even if status mode didn't change.\nWhen the checkbox isn't ticked, \"send times\" are counted from last status mode change (i.e., disabled state is more restrictive)."),
				IDC_MOREOPTDLG_EVNTMSG, LPGENW("Message"),
				IDC_MOREOPTDLG_EVNTFILE, LPGENW("File")
			};
			hWndTooltips = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, L"", WS_POPUP | TTS_NOPREFIX, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandleA("mir_app.mir"), nullptr);
			TOOLINFO ti = {};
			ti.cbSize = sizeof(ti);
			ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
			ti.hwnd = hwndDlg;
			for (auto &it: Tooltips) {
				ti.uId = (UINT_PTR)GetDlgItem(hwndDlg, it.m_dlgItemID);
				ti.lpszText = TranslateW(it.Text);
				SendMessage(hWndTooltips, TTM_ADDTOOL, 0, (LPARAM)&ti);
			}
			SendMessage(hWndTooltips, TTM_SETMAXTIPWIDTH, 0, 500);
			SendMessage(hWndTooltips, TTM_SETDELAYTIME, TTDT_AUTOPOP, 32767); // tooltip hide time; looks like 32 seconds is the maximum

			g_AutoreplyOptPage.DBToMemToPage();
			EnableAutoreplyOptDlgControls();
			ChangeLock--;
			MakeGroupCheckbox(GetDlgItem(hwndDlg, IDC_REPLYDLG_ENABLEREPLY));
		}
		return true;

	case UM_ICONSCHANGED:
		for (auto &it: Dlg3StatusButtons)
			SendDlgItemMessage(hwndDlg, it.DlgItem, BM_SETIMAGE, IMAGE_ICON, (LPARAM)Skin_LoadProtoIcon(nullptr, it.Status));

		variables_skin_helpbutton(hwndDlg, IDC_REPLYDLG_VARS);
		SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_EVNTMSG, BM_SETIMAGE, IMAGE_ICON, (LPARAM)Skin_LoadIcon(SKINICON_EVENT_MESSAGE));
		SendDlgItemMessage(hwndDlg, IDC_MOREOPTDLG_EVNTFILE, BM_SETIMAGE, IMAGE_ICON, (LPARAM)Skin_LoadIcon(SKINICON_EVENT_FILE));
		break;
	
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == PSN_APPLY) {
			g_AutoreplyOptPage.PageToMemToDB();
			return true;
		}
		break;

	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDC_REPLYDLG_ENABLEREPLY:
				EnableAutoreplyOptDlgControls();
				// go through
			case IDC_REPLYDLG_DONTSENDTOICQ:
			case IDC_REPLYDLG_DONTREPLYINVISIBLE:
			case IDC_REPLYDLG_ONLYCLOSEDDLGREPLY:
			case IDC_REPLYDLG_ONLYIDLEREPLY:
			case IDC_REPLYDLG_RESETCOUNTERWHENSAMEICON:
			case IDC_REPLYDLG_EVENTMSG:
			case IDC_REPLYDLG_EVENTFILE:
			case IDC_REPLYDLG_LOGREPLY:
			case IDC_REPLYDLG_DISABLE_ONL:
			case IDC_REPLYDLG_DISABLE_AWAY:
			case IDC_REPLYDLG_DISABLE_NA:
			case IDC_REPLYDLG_DISABLE_OCC:
			case IDC_REPLYDLG_DISABLE_DND:
			case IDC_REPLYDLG_DISABLE_FFC:
			case IDC_REPLYDLG_DISABLE_INV:
				if (!ChangeLock)
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, 0);
				break;
			case IDC_REPLYDLG_VARS:
				my_variables_showhelp(hwndDlg, IDC_REPLYDLG_PREFIX);
				break;
			}
			break;

		case EN_CHANGE:
			if ((LOWORD(wParam) == IDC_REPLYDLG_SENDCOUNT) || (LOWORD(wParam) == IDC_REPLYDLG_PREFIX)) {
				if (!ChangeLock && g_AutoreplyOptPage.GetWnd()) {
					if (LOWORD(wParam) == IDC_REPLYDLG_SENDCOUNT)
						EnableAutoreplyOptDlgControls();
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, 0);
				}
			}
			break;
		case CBN_SELCHANGE:
			if (LOWORD(wParam) == IDC_REPLYDLG_ONLYIDLEREPLY_COMBO)
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, 0);
			break;
		}
		break;

	case WM_DESTROY:
		g_AutoreplyOptPage.SetWnd(nullptr);
		if (s_hDrawFont)
			DeleteObject(s_hDrawFont);

		DestroyWindow(hWndTooltips);
		break;
	}
	return 0;
}

//================================================ Modern options ==============================================

static struct {
	int DlgItem, Status;
}
Dlg4DefMsgDlgItems[] = {
	{ IDC_MESSAGEDLG_DEF_ONL, ID_STATUS_ONLINE    },
	{ IDC_MESSAGEDLG_DEF_AWAY, ID_STATUS_AWAY     },
	{ IDC_MESSAGEDLG_DEF_NA, ID_STATUS_NA         },
	{ IDC_MESSAGEDLG_DEF_OCC, ID_STATUS_OCCUPIED  },
	{ IDC_MESSAGEDLG_DEF_DND, ID_STATUS_DND       },
	{ IDC_MESSAGEDLG_DEF_FFC, ID_STATUS_FREECHAT  },
	{ IDC_MESSAGEDLG_DEF_INV, ID_STATUS_INVISIBLE },
};

static struct {
	int DlgItem, IconIndex;
	wchar_t* Text;
}
Dlg4Buttons[] = {
	{ IDC_MESSAGEDLG_NEWMSG, IDI_NEWMESSAGE,  LPGENW("Create new message") },
	{ IDC_MESSAGEDLG_NEWCAT, IDI_NEWCATEGORY, LPGENW("Create new category") },
	{ IDC_MESSAGEDLG_DEL,    IDI_DELETE,      LPGENW("Delete") },
	{ IDC_MESSAGEDLG_VARS,   -1,              LPGENW("Open Variables help dialog") },
};

// ================================================ Contact list ================================================
// Based on the code from built-in Miranda ignore module

#define UM_CONTACTSDLG_RESETLISTOPTIONS (WM_USER + 20)

#define EXTRACOLUMNSCOUNT 3
#define IGNORECOLUMN 2
#define AUTOREPLYCOLUMN 1
#define NOTIFYCOLUMN 0

#define EXTRAICON_DOT 0
#define EXTRAICON_IGNORE 1
#define EXTRAICON_AUTOREPLYON 2
#define EXTRAICON_AUTOREPLYOFF 3
#define EXTRAICON_INDEFINITE 4

#define VAL_INDEFINITE (-2)

static WNDPROC g_OrigContactsProc;

__inline int DBValueToIgnoreIcon(int m_value)
{
	switch (m_value) {
		case VAL_INDEFINITE: return EXTRAICON_INDEFINITE;
		case 0: return EXTRAICON_DOT;
		default: return EXTRAICON_IGNORE;
	}
}

__inline int IgnoreIconToDBValue(int m_value)
{
	switch (m_value) {
		case EXTRAICON_DOT: return 0;
		case EXTRAICON_IGNORE: return 1;
		default: return VAL_INDEFINITE; // EXTRAICON_INDEFINITE and 0xFF
	}
}

__inline int DBValueToOptReplyIcon(int m_value)
{
	switch (m_value) {
		case VAL_INDEFINITE: return EXTRAICON_INDEFINITE;
		case VAL_USEDEFAULT: return EXTRAICON_DOT;
		case 0: return EXTRAICON_AUTOREPLYOFF;
		default: return EXTRAICON_AUTOREPLYON;
	}
}

__inline int ReplyIconToDBValue(int m_value)
{
	switch (m_value) {
		case EXTRAICON_DOT: return VAL_USEDEFAULT;
		case EXTRAICON_AUTOREPLYOFF: return 0;
		case EXTRAICON_AUTOREPLYON: return 1;
		default: return VAL_INDEFINITE; // EXTRAICON_INDEFINITE and 0xFF
	}
}

static void SetListGroupIcons(HWND hwndList, HANDLE hFirstItem, HANDLE hParentItem)
{
	int GroupIcons[EXTRACOLUMNSCOUNT] = { 0xFF, 0xFF, 0xFF };
	int FirstItemType = SendMessage(hwndList, CLM_GETITEMTYPE, (WPARAM)hFirstItem, 0);
	HANDLE hItem = (FirstItemType == CLCIT_GROUP) ? hFirstItem : (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXTGROUP, (LPARAM)hFirstItem);
	while (hItem) {
		HANDLE hChildItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hItem);
		if (hChildItem)
			SetListGroupIcons(hwndList, hChildItem, hItem);

		for (int i = 0; i < _countof(GroupIcons); i++) {
			int Icon = SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(i, 0));
			if (GroupIcons[i] == 0xFF)
				GroupIcons[i] = Icon;
			else if (Icon != 0xFF && GroupIcons[i] != Icon)
				GroupIcons[i] = EXTRAICON_INDEFINITE;
		}
		hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXTGROUP, (LPARAM)hItem);
	}
	
	hItem = (FirstItemType == CLCIT_CONTACT) ? hFirstItem : (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXTCONTACT, (LPARAM)hFirstItem);
	while (hItem) {
		for (int i = 0; i < _countof(GroupIcons); i++) {
			int Icon = SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(i, 0));
			if (GroupIcons[i] == 0xFF)
				GroupIcons[i] = Icon;
			else if (Icon != 0xFF && GroupIcons[i] != Icon)
				GroupIcons[i] = EXTRAICON_INDEFINITE;
		}
		hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXTCONTACT, (LPARAM)hItem);
	}
	
	// set icons
	for (int i = 0; i < _countof(GroupIcons); i++)
		SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hParentItem, MAKELPARAM(i, GroupIcons[i]));
}

static void SetAllChildIcons(HWND hwndList, HANDLE hFirstItem, int iColumn, int iImage)
{
	HANDLE hItem, hChildItem;
	int typeOfFirst = SendMessage(hwndList, CLM_GETITEMTYPE, (WPARAM)hFirstItem, 0);
	//check groups
	if (typeOfFirst == CLCIT_GROUP)
		hItem = hFirstItem;
	else
		hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXTGROUP, (LPARAM)hFirstItem);

	while (hItem) {
		hChildItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hItem);
		if (hChildItem)
			SetAllChildIcons(hwndList, hChildItem, iColumn, iImage);
		hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXTGROUP, (LPARAM)hItem);
	}
	//check contacts
	if (typeOfFirst == CLCIT_CONTACT)
		hItem = hFirstItem;
	else
		hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXTCONTACT, (LPARAM)hFirstItem);

	while (hItem) {
		int iOldIcon = SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, iColumn);
		if (iOldIcon != 0xFF && iOldIcon != iImage)
			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(iColumn, iImage));
		hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXTCONTACT, (LPARAM)hItem);
	}
}

static void SetIconsForColumn(HWND hwndList, HANDLE hItem, HANDLE hItemAll, int iColumn, int iImage)
{
	int itemType = SendMessage(hwndList, CLM_GETITEMTYPE, (WPARAM)hItem, 0);
	switch (itemType) {
	case CLCIT_CONTACT:
		{
			int oldiImage = SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, iColumn);
			if (oldiImage != 0xFF && oldiImage != iImage)
				SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(iColumn, iImage));
		}
		break;
	case CLCIT_INFO:
		if (hItem == hItemAll)
			SetAllChildIcons(hwndList, hItem, iColumn, iImage);
		else
			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(iColumn, iImage));
		break;
	case CLCIT_GROUP:
		hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hItem);
		if (hItem)
			SetAllChildIcons(hwndList, hItem, iColumn, iImage);
		break;
	}
}

static void SaveItemState(HWND hwndList, MCONTACT hContact, HANDLE hItem)
{
	int Ignore = IgnoreIconToDBValue(SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(IGNORECOLUMN, 0)));
	int Reply = ReplyIconToDBValue(SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(AUTOREPLYCOLUMN, 0)));
	if (Ignore != VAL_INDEFINITE)
		CContactSettings(ID_STATUS_ONLINE, hContact).Ignore = Ignore;

	if (Reply != VAL_INDEFINITE)
		CContactSettings(ID_STATUS_ONLINE, hContact).Autoreply = Reply;

	if (hContact != INVALID_CONTACT_ID && g_MoreOptPage.GetDBValueCopy(IDC_MOREOPTDLG_PERSTATUSPERSONALSETTINGS)) {
		int iMode;
		for (iMode = ID_STATUS_AWAY; iMode < ID_STATUS_MAX; iMode++) {
			if (Ignore != VAL_INDEFINITE)
				CContactSettings(iMode, hContact).Ignore = Ignore;

			// Notify is not per-status, so we're not setting it here
			if (Reply != VAL_INDEFINITE)
				CContactSettings(iMode, hContact).Autoreply = Reply;
		}
	}
}

static void SetAllContactIcons(HWND hwndList, HANDLE hItemUnknown)
{
	SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItemUnknown, MAKELPARAM(IGNORECOLUMN, DBValueToIgnoreIcon(CContactSettings(ID_STATUS_ONLINE, INVALID_CONTACT_ID).Ignore)));
	SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItemUnknown, MAKELPARAM(AUTOREPLYCOLUMN, DBValueToOptReplyIcon(CContactSettings(ID_STATUS_ONLINE, INVALID_CONTACT_ID).Autoreply)));

	for (auto &hContact : Contacts()) {
		HANDLE hItem = (HANDLE)SendMessage(hwndList, CLM_FINDCONTACT, hContact, 0);
		if (hItem) {
			int Ignore = CContactSettings(ID_STATUS_ONLINE, hContact).Ignore;
			int Reply = CContactSettings(ID_STATUS_ONLINE, hContact).Autoreply;
			if (g_MoreOptPage.GetDBValueCopy(IDC_MOREOPTDLG_PERSTATUSPERSONALSETTINGS)) {
				int iMode;
				for (iMode = ID_STATUS_AWAY; iMode < ID_STATUS_MAX; iMode++) {
					if (CContactSettings(iMode, hContact).Ignore != Ignore)
						Ignore = VAL_INDEFINITE;

					if (CContactSettings(iMode, hContact).Autoreply != Reply)
						Reply = VAL_INDEFINITE;
				}
			}
			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(IGNORECOLUMN, DBValueToIgnoreIcon(Ignore)));
			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(AUTOREPLYCOLUMN, DBValueToOptReplyIcon(Reply)));
		}
	}
}

static LRESULT CALLBACK ContactsSubclassProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg) {
	case WM_LBUTTONDBLCLK:
		uint32_t hitFlags;
		HANDLE hItem = (HANDLE)SendMessage(hWnd, CLM_HITTEST, (WPARAM)&hitFlags, lParam);
		if (hItem && (hitFlags & CLCHT_ONITEMEXTRA))
			Msg = WM_LBUTTONDOWN; // may be considered as a hack, but it's needed to make clicking on extra icons more convenient
		break;
	}
	return CallWindowProc(g_OrigContactsProc, hWnd, Msg, wParam, lParam);
}

INT_PTR CALLBACK ContactsOptDlg(HWND hwndDlg, UINT msg, WPARAM, LPARAM lParam)
{
	HWND hwndList = GetDlgItem(hwndDlg, IDC_CONTACTSDLG_LIST);
	static HANDLE hItemAll, hItemUnknown;
	
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		{
			MySetPos(hwndDlg);
			HIMAGELIST hIml = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 5, 2);
			ImageList_AddIcon(hIml, g_plugin.getIcon(IDI_DOT));
			ImageList_AddIcon(hIml, g_plugin.getIcon(IDI_IGNORE));
			ImageList_AddIcon(hIml, g_plugin.getIcon(IDI_SOE_ENABLED));
			ImageList_AddIcon(hIml, g_plugin.getIcon(IDI_SOE_DISABLED));
			ImageList_AddIcon(hIml, g_plugin.getIcon(IDI_INDEFINITE));
			
			SendMessage(hwndList, CLM_SETEXTRAIMAGELIST, 0, (LPARAM)hIml);
			SendMessage(hwndDlg, UM_CONTACTSDLG_RESETLISTOPTIONS, 0, 0);
			SendMessage(hwndList, CLM_SETEXTRACOLUMNS, EXTRACOLUMNSCOUNT, 0);

			SendDlgItemMessage(hwndDlg, IDC_SI_INDEFINITE, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_plugin.getIcon(IDI_INDEFINITE));
			SendDlgItemMessage(hwndDlg, IDC_SI_SOE_ENABLED, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_plugin.getIcon(IDI_SOE_ENABLED));
			SendDlgItemMessage(hwndDlg, IDC_SI_SOE_DISABLED, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_plugin.getIcon(IDI_SOE_DISABLED));
			SendDlgItemMessage(hwndDlg, IDC_SI_IGNORE, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_plugin.getIcon(IDI_IGNORE));
			
			CLCINFOITEM cii = { 0 };
			cii.cbSize = sizeof(cii);
			cii.flags = CLCIIF_GROUPFONT;
			cii.pszText = TranslateT("** All contacts **");
			hItemAll = (HANDLE)SendMessage(hwndList, CLM_ADDINFOITEM, 0, (LPARAM)&cii);
			cii.pszText = TranslateT("** Not-on-list contacts **"); // == Unknown contacts
			hItemUnknown = (HANDLE)SendMessage(hwndList, CLM_ADDINFOITEM, 0, (LPARAM)&cii);

			for (auto &hContact : Contacts()) {
				char *szProto = Proto_GetBaseAccountName(hContact);
				if (szProto) {
					int Flag1 = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0);
					if ((Flag1 & PF1_IM) != PF1_IM && !(Flag1 & PF1_INDIVMODEMSG)) // does contact's protocol supports message sending/receiving or individual status messages?
						SendMessage(hwndList, CLM_DELETEITEM, SendMessage(hwndList, CLM_FINDCONTACT, hContact, 0), 0);
				}
			}

			SetAllContactIcons(hwndList, hItemUnknown);
			SetListGroupIcons(hwndList, (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0), hItemAll);
			g_OrigContactsProc = (WNDPROC)SetWindowLongPtr(hwndList, GWLP_WNDPROC, (LONG_PTR)ContactsSubclassProc);
		}
		break;

	case UM_CONTACTSDLG_RESETLISTOPTIONS:
		SendMessage(hwndList, CLM_SETHIDEEMPTYGROUPS, 1, 0);
		break;

	case WM_SETFOCUS:
		SetFocus(hwndList);
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case IDC_CONTACTSDLG_LIST:
			switch (((LPNMHDR)lParam)->code) {
			case CLN_NEWCONTACT:
			case CLN_LISTREBUILT:
				SetAllContactIcons(hwndList, hItemUnknown);
				// fall through
			case CLN_CONTACTMOVED:
				SetListGroupIcons(hwndList, (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0), hItemAll);
				break;
			case CLN_OPTIONSCHANGED:
				SendMessage(hwndDlg, UM_CONTACTSDLG_RESETLISTOPTIONS, 0, 0);
				break;

			case NM_CLICK:
			case NM_DBLCLK:
				{
					NMCLISTCONTROL *nm = (NMCLISTCONTROL*)lParam;
					if (nm->iColumn == -1)
						break;

					uint32_t hitFlags;
					HANDLE hItem = (HANDLE)SendMessage(hwndList, CLM_HITTEST, (WPARAM)&hitFlags, MAKELPARAM(nm->pt.x, nm->pt.y));
					if (!hItem || !(hitFlags & CLCHT_ONITEMEXTRA))
						break;

					int iImage = SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(nm->iColumn, 0));
					switch (nm->iColumn) {
					case AUTOREPLYCOLUMN:
						switch (iImage) {
							case EXTRAICON_DOT: iImage = EXTRAICON_AUTOREPLYOFF; break;
							case EXTRAICON_AUTOREPLYOFF: iImage = EXTRAICON_AUTOREPLYON; break;
							default: iImage = EXTRAICON_DOT; // EXTRAICON_AUTOREPLYON and EXTRAICON_INDEFINITE
						}
						break;
					case IGNORECOLUMN:
						iImage = (iImage == EXTRAICON_DOT) ? EXTRAICON_IGNORE : EXTRAICON_DOT;
						break;
					}
				
					SetIconsForColumn(hwndList, hItem, hItemAll, nm->iColumn, iImage);
					SetListGroupIcons(hwndList, (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0), hItemAll);
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				}
			}
			break;

		case 0:
			switch (((LPNMHDR)lParam)->code) {
			case PSN_APPLY:
				for (auto &hContact : Contacts()) {
					HANDLE hItem = (HANDLE)SendMessage(hwndList, CLM_FINDCONTACT, hContact, 0);
					if (hItem)
						SaveItemState(hwndList, hContact, hItem);
				}

				SaveItemState(hwndList, INVALID_CONTACT_ID, hItemUnknown);
				return true;
			}
		}
		break;

	case WM_DESTROY:
		HIMAGELIST hIml = (HIMAGELIST)SendMessage(hwndList, CLM_GETEXTRAIMAGELIST, 0, 0);
		_ASSERT(hIml);
		ImageList_Destroy(hIml);
		break;
	}
	return 0;
}

int OptsDlgInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE optDi = { sizeof(optDi) };
	optDi.position = 920000000;
	optDi.flags = ODPF_BOLDGROUPS;

	optDi.szTitle.a = OPT_MAINGROUP;
	optDi.pfnDlgProc = MessagesOptDlg;
	optDi.pszTemplate = MAKEINTRESOURCEA(IDD_MESSAGES);
	optDi.szTab.a = LPGEN("Status messages");
	g_plugin.addOptions(wParam, &optDi);

	optDi.pfnDlgProc = MoreOptDlg;
	optDi.pszTemplate = MAKEINTRESOURCEA(IDD_MOREOPTDIALOG);
	optDi.szTab.a = LPGEN("Main options");
	g_plugin.addOptions(wParam, &optDi);

	optDi.pfnDlgProc = AutoreplyOptDlg;
	optDi.pszTemplate = MAKEINTRESOURCEA(IDD_AUTOREPLY);
	optDi.szTab.a = LPGEN("Autoreply");
	g_plugin.addOptions(wParam, &optDi);

	optDi.pfnDlgProc = ContactsOptDlg;
	optDi.pszTemplate = MAKEINTRESOURCEA(IDD_CONTACTSOPTDLG);
	optDi.szTab.a = LPGEN("Contacts");
	g_plugin.addOptions(wParam, &optDi);
	return 0;
}

COptPage g_SetAwayMsgPage(MODULENAME, nullptr);
COptPage g_MsgTreePage(MODULENAME, nullptr);

void InitOptions()
{
	g_MessagesOptPage.Items.AddElem(new COptItem_Generic(IDC_MESSAGEDLG_VARS, IDC_MESSAGEDLG_MSGTREE));
	g_MessagesOptPage.Items.AddElem(new COptItem_Generic(IDC_MESSAGEDLG_DEL));
	g_MessagesOptPage.Items.AddElem(new COptItem_Generic(IDC_MESSAGEDLG_MSGTITLE));
	g_MessagesOptPage.Items.AddElem(new COptItem_Generic(IDC_MESSAGEDLG_MSGDATA));
	g_MessagesOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MESSAGEDLG_DEF_ONL, nullptr, DBVT_BYTE, 0, 0, IDC_MESSAGEDLG_MSGTREE));
	g_MessagesOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MESSAGEDLG_DEF_AWAY, nullptr, DBVT_BYTE, 0, 0, IDC_MESSAGEDLG_MSGTREE));
	g_MessagesOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MESSAGEDLG_DEF_NA, nullptr, DBVT_BYTE, 0, 0, IDC_MESSAGEDLG_MSGTREE));
	g_MessagesOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MESSAGEDLG_DEF_OCC, nullptr, DBVT_BYTE, 0, 0, IDC_MESSAGEDLG_MSGTREE));
	g_MessagesOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MESSAGEDLG_DEF_DND, nullptr, DBVT_BYTE, 0, 0, IDC_MESSAGEDLG_MSGTREE));
	g_MessagesOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MESSAGEDLG_DEF_FFC, nullptr, DBVT_BYTE, 0, 0, IDC_MESSAGEDLG_MSGTREE));
	g_MessagesOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MESSAGEDLG_DEF_INV, nullptr, DBVT_BYTE, 0, 0, IDC_MESSAGEDLG_MSGTREE));

	TreeItemArray DefMsgTree;
	int ParentID1;
	int ID = 0;
	TreeRootItemArray RootItems;
	RootItems.AddElem(CTreeRootItem(TranslateT("Predefined messages"), g_Messages_PredefinedRootID = ID++, TIF_EXPANDED));
	RootItems.AddElem(CTreeRootItem(TranslateT("Recent messages"), g_Messages_RecentRootID = ID++, TIF_EXPANDED));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Gone fragging"), g_Messages_PredefinedRootID, ID++, 0, TranslateT("Been fragging since %nas_awaysince_time%, I'll message you later when the adrenaline wears off.")));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Creepy"), g_Messages_PredefinedRootID, ID++, 0, TranslateT("Your master, %nas_mynick%, has been %nas_statdesc% since the day that is only known as ?nas_awaysince_date(dddd)... When he gets back, i'll tell him you dropped by...")));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Default messages"), g_Messages_PredefinedRootID, ParentID1 = ID++, TIF_GROUP | TIF_EXPANDED));
	g_MsgTreePage.Items.AddElem(new COptItem_IntDBSetting(IDS_MESSAGEDLG_DEF_ONL, StatusToDBSetting(ID_STATUS_ONLINE, MESSAGES_DB_MSGTREEDEF), DBVT_WORD, false, ID));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Online"), ParentID1, ID++, 0, TranslateT("Yep, I'm here.")));
	g_MsgTreePage.Items.AddElem(new COptItem_IntDBSetting(IDS_MESSAGEDLG_DEF_AWAY, StatusToDBSetting(ID_STATUS_AWAY, MESSAGES_DB_MSGTREEDEF), DBVT_WORD, false, ID));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Away"), ParentID1, ID++, 0, TranslateT("Been gone since %nas_awaysince_time%, will be back later.")));
	g_MsgTreePage.Items.AddElem(new COptItem_IntDBSetting(IDS_MESSAGEDLG_DEF_NA, StatusToDBSetting(ID_STATUS_NA, MESSAGES_DB_MSGTREEDEF), DBVT_WORD, false, ID));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Not available"), ParentID1, ID++, 0, TranslateT("Give it up, I'm not in!")));
	g_MsgTreePage.Items.AddElem(new COptItem_IntDBSetting(IDS_MESSAGEDLG_DEF_OCC, StatusToDBSetting(ID_STATUS_OCCUPIED, MESSAGES_DB_MSGTREEDEF), DBVT_WORD, false, ID));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Occupied"), ParentID1, ID++, 0, TranslateT("Not right now.")));
	g_MsgTreePage.Items.AddElem(new COptItem_IntDBSetting(IDS_MESSAGEDLG_DEF_DND, StatusToDBSetting(ID_STATUS_DND, MESSAGES_DB_MSGTREEDEF), DBVT_WORD, false, ID));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Do not disturb"), ParentID1, ID++, 0, TranslateT("Give a guy some peace, would ya?")));
	g_MsgTreePage.Items.AddElem(new COptItem_IntDBSetting(IDS_MESSAGEDLG_DEF_FFC, StatusToDBSetting(ID_STATUS_FREECHAT, MESSAGES_DB_MSGTREEDEF), DBVT_WORD, false, ID));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Free for chat"), ParentID1, ID++, 0, TranslateT("I'm a chatbot!")));
	g_MsgTreePage.Items.AddElem(new COptItem_IntDBSetting(IDS_MESSAGEDLG_DEF_INV, StatusToDBSetting(ID_STATUS_INVISIBLE, MESSAGES_DB_MSGTREEDEF), DBVT_WORD, false, ID));
	DefMsgTree.AddElem(CTreeItem(TranslateT("Invisible"), ParentID1, ID++, 0, TranslateT("I'm hiding from the mafia.")));
	g_MsgTreePage.Items.AddElem(new COptItem_TreeCtrl(IDV_MSGTREE, "MsgTree", DefMsgTree, RootItems, 0, "Text"));

	g_SetAwayMsgPage.Items.AddElem(new COptItem_BitDBSetting(IDS_SAWAYMSG_SHOWMSGTREE, "SAMDlgFlags", DBVT_BYTE, DF_SAM_DEFDLGFLAGS, DF_SAM_SHOWMSGTREE));
	g_SetAwayMsgPage.Items.AddElem(new COptItem_BitDBSetting(IDS_SAWAYMSG_SHOWCONTACTTREE, "SAMDlgFlags", DBVT_BYTE, DF_SAM_DEFDLGFLAGS, DF_SAM_SHOWCONTACTTREE));
	g_SetAwayMsgPage.Items.AddElem(new COptItem_BitDBSetting(IDS_SAWAYMSG_AUTOSAVEDLGSETTINGS, "AutoSaveDlgSettings", DBVT_BYTE, 1));
	g_SetAwayMsgPage.Items.AddElem(new COptItem_BitDBSetting(IDS_SAWAYMSG_DISABLEVARIABLES, "DisableVariables", DBVT_BYTE, 0));

	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_PERSTATUSMRM, "PerStatusMRM", DBVT_BYTE, 0));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_RESETPROTOMSGS, "ResetProtoMsgs", DBVT_BYTE, 1));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_PERSTATUSPROTOMSGS, "PerStatusProtoMsgs", DBVT_BYTE, 0));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_PERSTATUSPROTOSETTINGS, "PerStatusProtoSettings", DBVT_BYTE, 0));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_SAVEPERSONALMSGS, "SavePersonalMsgs", DBVT_BYTE, 1));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_PERSTATUSPERSONAL, "PerStatusPersonal", DBVT_BYTE, 0));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_PERSTATUSPERSONALSETTINGS, "PerStatusPersonalSettings", DBVT_BYTE, 0));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_MYNICKPERPROTO, "MyNickPerProto", DBVT_BYTE, 1));
	g_MoreOptPage.Items.AddElem(new COptItem_IntEdit(IDC_MOREOPTDLG_WAITFORMSG, "WaitForMsg", DBVT_WORD, TRUE, 5));
	g_MoreOptPage.Items.AddElem(new COptItem_IntEdit(IDC_MOREOPTDLG_RECENTMSGSCOUNT, "MRMCount", DBVT_WORD, TRUE, 5));
	g_MoreOptPage.Items.AddElem(new COptItem_Radiobutton(IDC_MOREOPTDLG_USEDEFMSG, "UseByDefault", DBVT_BYTE, MOREOPTDLG_DEF_USEBYDEFAULT, 0));
	g_MoreOptPage.Items.AddElem(new COptItem_Radiobutton(IDC_MOREOPTDLG_USELASTMSG, "UseByDefault", DBVT_BYTE, MOREOPTDLG_DEF_USEBYDEFAULT, 1));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_UPDATEMSGS, "UpdateMsgs", DBVT_BYTE, 0));
	g_MoreOptPage.Items.AddElem(new COptItem_IntEdit(IDC_MOREOPTDLG_UPDATEMSGSPERIOD, "UpdateMsgsPeriod", DBVT_WORD, FALSE, 300));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_DONTPOPDLG_ONL, "DontPopDlg", DBVT_WORD, MOREOPTDLG_DEF_DONTPOPDLG, SF_ONL));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_DONTPOPDLG_AWAY, "DontPopDlg", DBVT_WORD, MOREOPTDLG_DEF_DONTPOPDLG, SF_AWAY));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_DONTPOPDLG_NA, "DontPopDlg", DBVT_WORD, MOREOPTDLG_DEF_DONTPOPDLG, SF_NA));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_DONTPOPDLG_OCC, "DontPopDlg", DBVT_WORD, MOREOPTDLG_DEF_DONTPOPDLG, SF_OCC));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_DONTPOPDLG_DND, "DontPopDlg", DBVT_WORD, MOREOPTDLG_DEF_DONTPOPDLG, SF_DND));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_DONTPOPDLG_FFC, "DontPopDlg", DBVT_WORD, MOREOPTDLG_DEF_DONTPOPDLG, SF_FFC));
	g_MoreOptPage.Items.AddElem(new COptItem_Checkbox(IDC_MOREOPTDLG_DONTPOPDLG_INV, "DontPopDlg", DBVT_WORD, MOREOPTDLG_DEF_DONTPOPDLG, SF_INV));

	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_ENABLEREPLY, DB_ENABLEREPLY, DBVT_BYTE, AUTOREPLY_DEF_REPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Generic(IDC_REPLYDLG_STATIC_ONEVENT, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_EVENTMSG, "ReplyOnEvent", DBVT_BYTE, AUTOREPLY_DEF_REPLYONEVENT, EF_MSG, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_EVENTFILE, "ReplyOnEvent", DBVT_BYTE, AUTOREPLY_DEF_REPLYONEVENT, EF_FILE, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DONTSENDTOICQ, "DontSendToICQ", DBVT_BYTE, 0, 0, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DONTREPLYINVISIBLE, "DontReplyInvisible", DBVT_BYTE, 1, 0, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_LOGREPLY, "LogReply", DBVT_BYTE, 1, 0, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_ONLYIDLEREPLY, "OnlyIdleReply", DBVT_BYTE, 0, 0, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_ONLYCLOSEDDLGREPLY, "OnlyClosedDlgReply", DBVT_BYTE, 1, 0, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Generic(IDC_REPLYDLG_STATIC_SEND, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_IntEdit(IDC_REPLYDLG_SENDCOUNT, "ReplyCount", DBVT_WORD, TRUE, -1, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Generic(IDC_REPLYDLG_STATIC_TIMES, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_RESETCOUNTERWHENSAMEICON, "ResetReplyCounterWhenSameIcon", DBVT_BYTE, 1, 0, IDC_REPLYDLG_SENDCOUNT));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Generic(IDC_REPLYDLG_STATIC_DISABLEWHENSTATUS, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DISABLE_ONL, "DisableReply", DBVT_WORD, AUTOREPLY_DEF_DISABLEREPLY, SF_ONL, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DISABLE_AWAY, "DisableReply", DBVT_WORD, AUTOREPLY_DEF_DISABLEREPLY, SF_AWAY, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DISABLE_NA, "DisableReply", DBVT_WORD, AUTOREPLY_DEF_DISABLEREPLY, SF_NA, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DISABLE_OCC, "DisableReply", DBVT_WORD, AUTOREPLY_DEF_DISABLEREPLY, SF_OCC, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DISABLE_DND, "DisableReply", DBVT_WORD, AUTOREPLY_DEF_DISABLEREPLY, SF_DND, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DISABLE_FFC, "DisableReply", DBVT_WORD, AUTOREPLY_DEF_DISABLEREPLY, SF_FFC, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Checkbox(IDC_REPLYDLG_DISABLE_INV, "DisableReply", DBVT_WORD, AUTOREPLY_DEF_DISABLEREPLY, SF_INV, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Generic(IDC_REPLYDLG_STATIC_FORMAT, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Edit(IDC_REPLYDLG_PREFIX, "ReplyPrefix", AWAY_MSGDATA_MAX, AUTOREPLY_DEF_PREFIX, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Generic(IDC_REPLYDLG_VARS, IDC_REPLYDLG_ENABLEREPLY));
	g_AutoreplyOptPage.Items.AddElem(new COptItem_Generic(IDC_REPLYDLG_STATIC_EXTRATEXT, IDC_REPLYDLG_ENABLEREPLY));
}
