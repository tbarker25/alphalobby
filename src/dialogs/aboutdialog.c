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

#include <windows.h>
#include <richedit.h>
#include <shellapi.h>

#include "aboutdialog.h"
#include "../resource.h"
#include "../mainwindow.h"
#include "../common.h"

static BOOL CALLBACK about_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);

void
AboutDialog_create(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_ABOUT), g_main_window, about_proc);
}

static BOOL CALLBACK
about_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		HWND edit_box = GetDlgItem(window, IDC_ABOUT_EDIT);
		SendMessage(edit_box, EM_AUTOURLDETECT, 1, 0);
		SendMessage(edit_box, EM_SETEVENTMASK, 0, ENM_LINK | ENM_MOUSEEVENTS | ENM_SCROLL);
		SETTEXTEX set_text_info = {0, 0};
		SendMessage(edit_box, EM_SETTEXTEX, (WPARAM)&set_text_info,
				(LPARAM)"{\\rtf {\\b Alphalobby} {\\i " STRINGIFY(VERSION) "}\\par\\par "
				"{\\b Author:}\\par Thomas Barker (Axiomatic)\\par \\par "
				"{\\b Homepage:}\\par http://code.google.com/p/alphalobby/\\par \\par "
				"{\\b Flag icons:}\\par Mark James http://www.famfamfam.com/\\par }");
		return 0;
	}
	case WM_NOTIFY:
	{
		ENLINK *s = (void *)l_param;
		if (s->nmhdr.idFrom == IDC_ABOUT_EDIT && s->nmhdr.code == EN_LINK && s->msg == WM_LBUTTONUP) {
			SendMessage(s->nmhdr.hwndFrom, EM_EXSETSEL, 0, (LPARAM)&s->chrg);
			wchar_t url[256];
			SendMessage(s->nmhdr.hwndFrom, EM_GETTEXTEX,
					(WPARAM)&(GETTEXTEX){.cb = sizeof(url), .flags = GT_SELECTION, .codepage = 1200},
					(LPARAM)url);
			ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
		}
		return 0;
	}
	case WM_COMMAND:
		if (w_param == MAKEWPARAM(IDCANCEL, BN_CLICKED))
			EndDialog(window, 0);
		return 0;
	}
	return 0;
}
