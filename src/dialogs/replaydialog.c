/*
 * Copyright (c) 2013, Thomas Barker
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * It under the terms of the GNU General Public License as published by
 * The Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * Along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include <windows.h>
#include <commctrl.h>

#include "replaydialog.h"
#include "../resource.h"
#include "../mainwindow.h"
#include "../spring.h"
#include "../sync.h"

#define LENGTH(x) (sizeof x / sizeof *x)

static void activate_replay_list_item(HWND list_view_window, int index);
static BOOL CALLBACK replay_list_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);

void
ReplayDialog_create(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_REPLAY), g_main_window,
	    (DLGPROC)replay_list_proc);
}

static void
activate_replay_list_item(HWND list_view_window, int index)
{
	printf("index = %d\n", index);
	wchar_t buf[1024];
	LVITEM item = {.pszText = buf, LENGTH(buf)};
	SendMessage(list_view_window, LVM_GETITEMTEXT, (uintptr_t)index, (intptr_t)&item);
	wprintf(L"buf = %s\n", buf);
	Spring_launch_replay(buf);
}

static BOOL CALLBACK
replay_list_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	HWND list_window;
	switch (msg) {
	case WM_INITDIALOG:
		list_window = GetDlgItem(window, IDC_REPLAY_LIST);
		SendMessage(list_window, LVM_INSERTCOLUMN, 0,
				(intptr_t)&(LVCOLUMN){LVCF_TEXT, .pszText = (wchar_t *)L"filename"});
		SendDlgItemMessage(window, IDC_REPLAY_LIST, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
		Sync_add_replays_to_listview(list_window);
		SendDlgItemMessage(window, IDC_REPLAY_LIST, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
		return 0;
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			list_window = GetDlgItem(window, IDC_REPLAY_LIST);
			activate_replay_list_item(list_window, SendMessage(list_window, LVM_GETNEXTITEM, (uintptr_t)-1, LVNI_SELECTED));
			//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 0;
		} break;
	case WM_NOTIFY:
		if (((NMHDR *)l_param)->code == LVN_ITEMACTIVATE) {
			activate_replay_list_item(((NMITEMACTIVATE *)l_param)->hdr.hwndFrom, ((NMITEMACTIVATE *)l_param)->iItem);
			EndDialog(window, 0);
		}
		break;
	}
	return 0;
}
