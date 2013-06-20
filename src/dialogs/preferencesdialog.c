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

#include <stdbool.h>
#include <stdlib.h>

#include <windows.h>

#include "preferencesdialog.h"
#include "../mainwindow.h"
#include "../settings.h"
#include "../resource.h"

static BOOL CALLBACK preferences_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);

void
PreferencesDialog_create(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_PREFERENCES), g_main_window, preferences_proc);
}

static BOOL CALLBACK
preferences_proc(HWND window, UINT msg, WPARAM w_param,
		__attribute__((unused)) LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		HWND autoconnect = GetDlgItem(window, IDC_PREFERENCES_AUTOCONNECT);
		SendMessage(autoconnect, BM_SETCHECK, g_settings.flags & SETTING_AUTOCONNECT, 0);
		ShowWindow(autoconnect, !!Settings_load_str("password"));
		SetDlgItemTextA(window, IDC_PREFERENCES_PATH, g_settings.spring_path);
		return 0;
	}
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
		{
			char buf[1024];
			GetDlgItemTextA(window, IDC_PREFERENCES_PATH, buf, sizeof(buf));
			free(g_settings.spring_path);
			g_settings.spring_path = _strdup(buf);
			g_settings.flags = (g_settings.flags & ~SETTING_AUTOCONNECT) | SETTING_AUTOCONNECT * SendDlgItemMessage(window, IDC_PREFERENCES_AUTOCONNECT, BM_GETCHECK, 0, 0);
		}	//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 1;

		case MAKEWPARAM(IDC_PREFERENCES_RESET, BN_CLICKED):
			if (IDYES == MessageBox(window, L"This action cannot be reversed.", L"Reset Preferences", MB_YESNO)) {
				Settings_reset();
				SendMessage(window, WM_INITDIALOG, 0, 0);
			}
			return 1;
		} break;
	}
	return 0;
}
