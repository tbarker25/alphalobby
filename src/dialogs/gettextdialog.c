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

#include <inttypes.h>

#include <windows.h>

#include "gettextdialog.h"
#include "../resource.h"
#include "../mainwindow.h"

static BOOL CALLBACK get_text_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);

intptr_t
GetTextDialog2_create(HWND window, const char *title, char *buf, size_t buf_len)
{
	const char *param[] = {title, buf, (void *)buf_len};
	return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_GETTEXT), window,
	    (DLGPROC)get_text_proc, (intptr_t)param);
}

intptr_t
GetTextDialog_create(const char *title, char *buf, size_t buf_len)
{
	const char *param[] = {title, buf, (void *)buf_len};
	return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_GETTEXT),
	    g_main_window, (DLGPROC)get_text_proc, (intptr_t)param);
}

static BOOL CALLBACK
get_text_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		HWND text_box;
		const char **s = (void *)l_param;

		SetWindowTextA(window, s[0]);
		text_box = GetDlgItem(window, IDC_GETTEXT);
		SetWindowTextA(text_box, s[1]);
		SetWindowLongPtr(text_box, GWLP_USERDATA, (intptr_t)s[2]);
		SetWindowLongPtr(window, GWLP_USERDATA, (intptr_t)s[1]);
		return 0;
	}
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED): {
			char *buf;

			buf = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
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
