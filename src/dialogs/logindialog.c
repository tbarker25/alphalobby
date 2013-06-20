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

#include "gettextdialog.h"
#include "logindialog.h"
#include "../common.h"
#include "../resource.h"
#include "../mainwindow.h"
#include "../spring.h"
#include "../sync.h"
#include "../settings.h"
#include "../md5.h"
#include "../wincommon.h"
#include "../tasserver.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

static BOOL CALLBACK login_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static int on_login(HWND window, int is_registering);

__attribute__((pure))
static int
is_md5(const char *hash)
{
	if (strlen(hash) != 24)
		return 0;
	for (const char *c = hash; *c; ++c)
		if (!isalnum(*c) && *c != '+' && *c != '/' && *c != '=')
			return 0;
	return 1;
}

static void
to_md5(char *password)
{
	if (strlen(password) != 24)
		goto doit;
	for (const char *c = password; *c; ++c)
		if (!isalnum(*c) && *c != '+' && *c != '/' && *c != '=')
			goto doit;
	return;
	doit:
	strcpy(password, MD5_calc_checksum_base_64(password, strlen(password)));
}

static void
to_md5_2(char *md5, const char *password)
{
	memcpy(md5, is_md5(password) ? password : MD5_calc_checksum_base_64(password, strlen(password)), BASE16_MD5_LENGTH);
	md5[BASE16_MD5_LENGTH] = 0;
}

void
LoginDialog_create(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_LOGIN), g_main_window, login_proc);
}

static int
on_login(HWND window, int is_registering)
{
	char username[MAX_NAME_LENGTH+1];
	if (!GetDlgItemTextA(window, IDC_LOGIN_USERNAME, username, LENGTH(username))) {
		MessageBox(window, L"Enter a username less than "
				STRINGIFY(MAX_NAME_LENGTH) L" characters.",
				L"_username too long", MB_OK);
		return 1;
	}

	char password[BASE16_MD5_LENGTH+1];
	to_md5_2(password, GetDlgItemTextA2(window, IDC_LOGIN_PASSWORD));


	if (SendDlgItemMessage(window, IDC_LOGIN_SAVE, BM_GETCHECK, 0, 0)) {
		Settings_save_str("username", username);
		Settings_save_str("password", password);
	}
	g_settings.flags = (g_settings.flags & ~SETTING_AUTOCONNECT) | SETTING_AUTOCONNECT * SendDlgItemMessage(window, IDC_LOGIN_AUTOCONNECT, BM_GETCHECK, 0, 0);

	if (is_registering) {
		TasServer_send_login(username, password);
		return 0;

	}

	char confirm_password[sizeof(password)];
retry:
	confirm_password[0] = 0;
	if (GetTextDialog2_create(window, "Confirm password", confirm_password,
		LENGTH(confirm_password)))
		return 1;

	to_md5(confirm_password);
	if (strcmp(password, confirm_password)) {
		MessageBox(window, L"_passwords do not match.", L"Can not register new account", 0);
		goto retry;
	}

	TasServer_send_register(username, password);
	return 0;
}

static BOOL CALLBACK
login_proc(HWND window, UINT msg, WPARAM w_param,
		__attribute__((unused)) LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
		SetDlgItemText(window, IDC_LOGIN_USERNAME, utf8to16(Settings_load_str("username")));
		char *pw = Settings_load_str("password");
		if (pw) {
			SetDlgItemText(window, IDC_LOGIN_PASSWORD, utf8to16(pw));
			SendDlgItemMessage(window, IDC_LOGIN_SAVE, BM_SETCHECK, 1, 0);
			SendDlgItemMessage(window, IDC_LOGIN_AUTOCONNECT,
			    BM_SETCHECK, g_settings.flags & SETTING_AUTOCONNECT, 0);
		} else
			EnableWindow(GetDlgItem(window, IDC_LOGIN_AUTOCONNECT), 0);
		return 1;
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
		case MAKEWPARAM(IDC_LOGIN_REGISTER, BN_CLICKED):
			if (on_login(window, w_param == MAKEWPARAM(IDOK, BN_CLICKED)))
				return 1;
			//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 1;
		case MAKEWPARAM(IDC_LOGIN_SAVE, BN_CLICKED):
			EnableWindow(GetDlgItem(window, IDC_LOGIN_AUTOCONNECT),
			    SendDlgItemMessage(window, IDC_LOGIN_SAVE, BM_GETCHECK, 0, 0));
			return 1;
		} break;
	}
	return 0;
}
