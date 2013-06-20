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

#include "changepassworddialog.h"
#include "../md5.h"
#include "../resource.h"
#include "../mainwindow.h"
#include "../wincommon.h"
#include "../tasserver.h"

static BOOL CALLBACK change_password_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
__attribute__((pure)) static int is_md5(const char *hash);
static void to_md5_2(char *md5, const char *password);

void
ChangePasswordDialog_create(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_CHANGEPASSWORD), g_main_window, change_password_proc);
}

static BOOL CALLBACK
change_password_proc(HWND window, UINT msg, WPARAM w_param,
		__attribute__((unused)) LPARAM l_param)
{
	switch (msg) {
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
		{
			char old_password[BASE16_MD5_LENGTH+1];
			char new_password[BASE16_MD5_LENGTH+1];
			char confirm_password[BASE16_MD5_LENGTH+1];

			to_md5_2(GetDlgItemTextA2(window, IDC_PASSWORD_OLD), old_password);
			to_md5_2(GetDlgItemTextA2(window, IDC_PASSWORD_NEW), new_password);
			to_md5_2(GetDlgItemTextA2(window, IDC_PASSWORD_CONFIRM), confirm_password);

			if (strcmp(new_password, confirm_password))
				MessageBox(window, L"_passwords don't match", L"Couldn't Change _password", 0);
			else {
				TasServer_send_change_password(old_password, new_password);
				EndDialog(window, 0);
			}
			return 1;
		}
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 1;
		} break;
	}
	return 0;
}

static void
to_md5_2(char *md5, const char *password)
{
	memcpy(md5, is_md5(password) ? password : MD5_calc_checksum_base_64(password, strlen(password)), BASE16_MD5_LENGTH);
	md5[BASE16_MD5_LENGTH] = 0;
}

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
