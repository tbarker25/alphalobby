/* * Copyright (c) 2013, Thomas Barker
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

#include "hostdialog.h"
#include "../common.h"
#include "../host_relay.h"
#include "../mainwindow.h"
#include "../mybattle.h"
#include "../resource.h"
#include "../settings.h"
#include "../tasserver.h"

#define LENGTH(x) (sizeof x / sizeof *x)

static void on_host_init(HWND window);
static int on_host_ok(HWND window);
static BOOL CALLBACK host_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);

void
HostDialog_create(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_HOST), g_main_window,
	    (DLGPROC)host_proc);
}

static void
on_host_init(HWND window)
{
	const char *def;
	int defIndex;

	SetDlgItemText(window, IDC_HOST_DESCRIPTION, utf8to16(Settings_load_str("last_host_description")));
	def = Settings_load_str("last_host_mod"); defIndex=0;
	for (int i=0; i<g_mod_len; ++i) {
		if (def && !strcmp(def, g_mods[i]))
			defIndex = i;
		SendDlgItemMessageA(window, IDC_HOST_MOD, CB_ADDSTRING, 0, (intptr_t)g_mods[i]);
	}
	SendDlgItemMessage(window, IDC_HOST_MOD, CB_SETCURSEL, (uintptr_t)defIndex, 0);

	def = Settings_load_str("last_host_map"); defIndex=0;
	for (int i=0; i<g_map_len; ++i) {
		if (def && !strcmp(def, g_maps[i]))
			defIndex = i;
		SendDlgItemMessageA(window, IDC_HOST_MAP, CB_ADDSTRING, 0, (intptr_t)g_maps[i]);
	}
	SendDlgItemMessage(window, IDC_HOST_MAP, CB_SETCURSEL, (uintptr_t)defIndex, 0);

	SetDlgItemInt(window, IDC_HOST_PORT, 8452, 0);
	SendDlgItemMessage(window, IDC_HOST_USERELAY, BM_SETCHECK, 1, 0);

	def = Settings_load_str("last_host_manager"); defIndex=0;
	for (int i=1; i<g_relay_manager_len; ++i) {
		if (def && !strcmp(def, g_relay_managers[i]))
			defIndex = i;
		SendDlgItemMessage(window, IDC_HOST_RELAY, CB_ADDSTRING, 0, (intptr_t)utf8to16(g_relay_managers[i]));
	}
	SendDlgItemMessage(window, IDC_HOST_RELAY, CB_SETCURSEL, (uintptr_t)defIndex, 0);
}

static int
on_host_ok(HWND window)
{
	char description[128], mod[128], password[128], map[128];

	GetDlgItemTextA(window, IDC_HOST_DESCRIPTION, description, LENGTH(description));
	GetDlgItemTextA(window, IDC_HOST_PASSWORD, password, LENGTH(password));
	GetDlgItemTextA(window, IDC_HOST_MOD, mod, LENGTH(mod));
	GetDlgItemTextA(window, IDC_HOST_MAP, map, LENGTH(map));


	if (strchr(password, ' ')) {
		SetDlgItemText(window, IDC_HOST_PASSWORD, NULL);
		MessageBox(window, L"Please enter a password without spaces", L"Spaces are not allowed in passwords", 0);
		return 1;
	}
	Settings_save_str("last_host_description", description);
	Settings_save_str("last_host_mod", mod);
	Settings_save_str("last_host_map", map);
	if (SendDlgItemMessage(window, IDC_HOST_USERELAY, BM_GETCHECK, 0, 0)) {
		char manager[128];
		GetDlgItemTextA(window, IDC_HOST_RELAY, manager, LENGTH(manager));
		Settings_save_str("last_host_manager", manager);
		RelayHost_open_battle(description, password, mod, map, manager);
	} else {
		uint16_t port;
		port = (uint16_t)GetDlgItemInt(window, IDC_HOST_PORT, NULL, 0);
		if (!port) {
			if (MessageBox(window, L"Use the default port? (8452)", L"Can't _proceed Without a Valid Port", MB_YESNO) == IDYES)
				port = 8452;
			else
				return 1;
		}
		TasServer_send_open_battle(*password ? password : NULL, port, mod, map, description);
	}
	return 0;
}

static BOOL CALLBACK
host_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
		on_host_init(window);
		return 1;

	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			if (on_host_ok(window))
				return 1;
			//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 1;
		case MAKEWPARAM(IDC_HOST_USERELAY, BN_CLICKED):
		{
			int checked = SendMessage((HWND)l_param, BM_GETCHECK, 0, 0);
			EnableWindow(GetDlgItem(window, IDC_HOST_RELAY), checked);
			EnableWindow(GetDlgItem(window, IDC_HOST_PORT), !checked);
			EnableWindow(GetDlgItem(window, IDC_HOST_PORT_L), !checked);
			return 0;
		}
		}
	}
	return 0;
}
