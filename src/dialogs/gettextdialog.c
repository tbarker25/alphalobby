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

#include <windows.h>

#include "gettextdialog.h"
#include "../resource.h"
#include "../mainwindow.h"

static BOOL CALLBACK get_text_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);

LPARAM
GetTextDialog2_create(HWND window, const char *title, char *buf, size_t buf_len)
{
	const char *param[] = {title, buf, (void *)buf_len};
	return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_GETTEXT), window,
	    get_text_proc, (LPARAM)param);
}

LPARAM
GetTextDialog_create(const char *title, char *buf, size_t buf_len)
{
	const char *param[] = {title, buf, (void *)buf_len};
	return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_GETTEXT),
	    g_main_window, get_text_proc, (LPARAM)param);
}

static BOOL CALLBACK
get_text_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		const char **s = (void *)l_param;
		SetWindowTextA(window, s[0]);
		HWND textbox = GetDlgItem(window, IDC_GETTEXT);
		SetWindowTextA(textbox, s[1]);
		SetWindowLongPtr(textbox, GWLP_USERDATA, (LPARAM)s[2]);
		SetWindowLongPtr(window, GWLP_USERDATA, (LPARAM)s[1]);
		return 0;
	}
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED): {
			char *buf = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
			GetDlgItemTextA(window, IDC_GETTEXT, buf, GetWindowLongPtr(GetDlgItem(window, IDC_GETTEXT), GWLP_USERDATA));
			EndDialog(window, 0);
			return 1;
		}
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 1);
			return 1;
		}
		break;
	}
	return 0;
}
