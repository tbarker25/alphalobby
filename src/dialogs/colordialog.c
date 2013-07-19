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
#include <commctrl.h>

#include "colordialog.h"
#include "../common.h"
#include "../resource.h"
#include "../mainwindow.h"
#include "../user.h"

static COLORREF get_rgb(HWND window);
static BOOL CALLBACK color_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);

void
ColorDialog_create(UserBot *u)
{
	DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_COLOR), g_main_window,
	    (DLGPROC)color_proc, (intptr_t)u);
}

static COLORREF
get_rgb(HWND window)
{
	return RGB(SendDlgItemMessage(window, IDC_COLOR_R_L, UDM_GETPOS, 0, 0),
			SendDlgItemMessage(window, IDC_COLOR_G_L, UDM_GETPOS, 0, 0),
			SendDlgItemMessage(window, IDC_COLOR_B_L, UDM_GETPOS, 0, 0));
}

static BOOL CALLBACK
color_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		UserBot *s = (void *)l_param;
		SetDlgItemInt(window, IDC_COLOR_R, s->red, 0);
		SetDlgItemInt(window, IDC_COLOR_G, s->green, 0);
		SetDlgItemInt(window, IDC_COLOR_B, s->blue, 0);
		SendDlgItemMessage(window, IDC_COLOR_R_L, UDM_SETRANGE, 0, MAKELPARAM(255,0));
		SendDlgItemMessage(window, IDC_COLOR_G_L, UDM_SETRANGE, 0, MAKELPARAM(255,0));
		SendDlgItemMessage(window, IDC_COLOR_B_L, UDM_SETRANGE, 0, MAKELPARAM(255,0));
		SetWindowLongPtr(window, GWLP_USERDATA, l_param);
		return 1;
	}
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDC_COLOR_R, EN_CHANGE):
		case MAKEWPARAM(IDC_COLOR_G, EN_CHANGE):
		case MAKEWPARAM(IDC_COLOR_B, EN_CHANGE):
		{
			BOOL a;
			if (GetDlgItemInt(window, LOWORD(w_param), &a, 0)  > 255)
				SetDlgItemInt(window, LOWORD(w_param), 255, 0);
			else if (!a)
				SetDlgItemInt(window, LOWORD(w_param), 0, 0);
			else if (GetWindowLongPtr(window, GWLP_USERDATA)) //Make sure WM_INITDIALOG has been called so IDC_COLOR_PREVIEW actually exists
				InvalidateRect(GetDlgItem(window, IDC_COLOR_PREVIEW), NULL, 0);
			return 0;
		}
		case MAKEWPARAM(IDOK, BN_CLICKED):
		{
			/* TODO */
			/* UserBot *s = (void *)GetWindowLongPtr(window, GWLP_USERDATA); */
			/* uint32_t color = get_rgb(window); */
			/* TasServer_send_my_color(s, color); */
		} // fallthrough:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 0;
		}
		break;
	case WM_CTLCOLORSTATIC:
		return (GetDlgCtrlID((HWND)l_param) == IDC_COLOR_PREVIEW) * (intptr_t)CreateSolidBrush(get_rgb(window));
	}
	return 0;
}
