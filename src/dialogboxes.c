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

#include <ctype.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <zlib.h>

#include <windows.h>
#include <Commctrl.h>
#include <Richedit.h>
#include <basetyps.h>
#include <shellapi.h>

#include "battle.h"
#include "client_message.h"
#include "common.h"
#include "dialogboxes.h"
#include "downloader.h"
#include "host_relay.h"
#include "mainwindow.h"
#include "md5.h"
#include "mybattle.h"
#include "resource.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "user.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

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
to_md5_2(char *md5, const char *password)
{
	memcpy(md5, is_md5(password) ? password : MD5_calc_checksum_base_64(password, strlen(password)), BASE16_MD5_LENGTH);
	md5[BASE16_MD5_LENGTH] = 0;
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
		Login(username, password);
		return 0;

	}

	char confirm_password[sizeof(password)];
retry:
	confirm_password[0] = 0;
	if (GetTextDlg2(window, "Confirm password", confirm_password, LENGTH(confirm_password)))
		return 1;

	to_md5(confirm_password);
	if (strcmp(password, confirm_password)) {
		MessageBox(window, L"_passwords do not match.", L"Can not register new account", 0);
		goto retry;
	}

	RegisterAccount(username, password);
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
			SendDlgItemMessage(window, IDC_LOGIN_AUTOCONNECT, BM_SETCHECK, g_settings.flags & SETTING_AUTOCONNECT, 0);
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
			EnableWindow(GetDlgItem(window, IDC_LOGIN_AUTOCONNECT), SendDlgItemMessage(window, IDC_LOGIN_SAVE, BM_GETCHECK, 0, 0));
			return 1;
		} break;
	}
	return 0;
}

static void
on_host_init(HWND window)
{
	SetDlgItemText(window, IDC_HOST_DESCRIPTION, utf8to16(Settings_load_str("last_host_description")));
	const char *def; int defIndex;

	def = Settings_load_str("last_host_mod"); defIndex=0;
	for (size_t i=0; i<g_mod_len; ++i) {
		if (def && !strcmp(def, g_mods[i]))
			defIndex = i;
		SendDlgItemMessageA(window, IDC_HOST_MOD, CB_ADDSTRING, 0, (LPARAM)g_mods[i]);
	}
	SendDlgItemMessage(window, IDC_HOST_MOD, CB_SETCURSEL, defIndex, 0);

	def = Settings_load_str("last_host_map"); defIndex=0;
	for (size_t i=0; i<g_map_len; ++i) {
		if (def && !strcmp(def, g_maps[i]))
			defIndex = i;
		SendDlgItemMessageA(window, IDC_HOST_MAP, CB_ADDSTRING, 0, (LPARAM)g_maps[i]);
	}
	SendDlgItemMessage(window, IDC_HOST_MAP, CB_SETCURSEL, defIndex, 0);

	SetDlgItemInt(window, IDC_HOST_PORT, 8452, 0);
	SendDlgItemMessage(window, IDC_HOST_USERELAY, BM_SETCHECK, 1, 0);

	def = Settings_load_str("last_host_manager"); defIndex=0;
	for (int i=1; i<g_relay_manager_len; ++i) {
		if (def && !strcmp(def, g_relay_managers[i]))
			defIndex = i;
		SendDlgItemMessage(window, IDC_HOST_RELAY, CB_ADDSTRING, 0, (LPARAM)utf8to16(g_relay_managers[i]));
	}
	SendDlgItemMessage(window, IDC_HOST_RELAY, CB_SETCURSEL, defIndex, 0);
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
		port = GetDlgItemInt(window, IDC_HOST_PORT, NULL, 0);
		if (!port) {
			if (MessageBox(window, L"Use the default port? (8452)", L"Can't _proceed Without a Valid Port", MB_YESNO) == IDYES)
				port = 8452;
			else
				return 1;
		}
		OpenBattle(description, password, mod, map, port);
	}
	return 0;
}

static BOOL CALLBACK
host_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
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

static COLORREF
get_rgb(HWND window)
{
	return RGB(SendDlgItemMessage(window, IDC_COLOR_R_L, UDM_GETPOS, 0, 0),
			SendDlgItemMessage(window, IDC_COLOR_G_L, UDM_GETPOS, 0, 0),
			SendDlgItemMessage(window, IDC_COLOR_B_L, UDM_GETPOS, 0, 0));
}
static BOOL CALLBACK
color_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		union UserOrBot *s = (void *)l_param;
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
			union UserOrBot *s = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
			uint32_t color = get_rgb(window);
			SetColor(s, color);
		} // fallthrough:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 0;
		}
		break;
	case WM_CTLCOLORSTATIC:
		return (GetDlgCtrlID((HWND)l_param) == IDC_COLOR_PREVIEW) * (INT_PTR)CreateSolidBrush(get_rgb(window));
	}
	return 0;
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
		case MAKEWPARAM(IDOK, BN_CLICKED):
		{
			char *buf = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
			GetDlgItemTextA(window, IDC_GETTEXT, buf, GetWindowLongPtr(GetDlgItem(window, IDC_GETTEXT), GWLP_USERDATA));
			EndDialog(window, 0);
			return 1;
		}
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 1);
			return 1;
		} break;
	}
	return 0;
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
				Change_password(old_password, new_password);
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

static BOOL CALLBACK
agreement_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		DWORD CALLBACK edit_stream_callback(
				__attribute__((unused)) DWORD_PTR dwCookie,
				LPBYTE lp_buff, LONG cb, PLONG pcb)
		{
			*pcb = fread(lp_buff, 1, cb, (FILE *)l_param);
			return 0;
		}

		SendDlgItemMessage(window, IDC_AGREEMENT_TEXT, EM_STREAMIN, SF_RTF, (LPARAM)&(EDITSTREAM){.pfnCallback = edit_stream_callback});
		break;
	}
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			ConfirmAgreement();
			//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
		} break;
	}
	return 0;
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
				"{\\b Author:}\\par Tom Barker (Axiomatic)\\par \\par "
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

static void
activate_replay_list_item(HWND list_view_window, int index)
{
	printf("index = %d\n", index);
	wchar_t buf[1024];
	LVITEM item = {.pszText = buf, LENGTH(buf)};
	SendMessage(list_view_window, LVM_GETITEMTEXT, index, (LPARAM)&item);
	wprintf(L"buf = %s\n", buf);
	Spring_launch_replay(buf);
}

static BOOL CALLBACK replay_list_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	HWND list_window;
	switch (msg) {
	case WM_INITDIALOG:
		list_window = GetDlgItem(window, IDC_REPLAY_LIST);
		SendMessage(list_window, LVM_INSERTCOLUMN, 0,
				(LPARAM)&(LVCOLUMN){LVCF_TEXT, .pszText = L"filename"});
		SendDlgItemMessage(window, IDC_REPLAY_LIST, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
		Sync_add_replays_to_listview(list_window);
		SendDlgItemMessage(window, IDC_REPLAY_LIST, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
		return 0;
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			list_window = GetDlgItem(window, IDC_REPLAY_LIST);
			activate_replay_list_item(list_window, SendMessage(list_window, LVM_GETNEXTITEM, -1, LVNI_SELECTED));
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

static void
rapidAdd_available(HWND window, char download_only)
{
#define TVGN_NEXTSELECTED       0x000B
	HTREEITEM item = (HTREEITEM)SendDlgItemMessage(window, IDC_RAPID_AVAILABLE, TVM_GETNEXTITEM, TVGN_NEXTSELECTED, (LPARAM)NULL);
#undef TVGN_NEXTSELECTED

	char item_text1[256];
	char item_text2[256];
	char *text_a = item_text1;
	char *text_b = NULL;
	while (item) {
		TVITEMA item_info = {TVIF_HANDLE | TVIF_TEXT | TVIF_CHILDREN, item,
			.pszText = text_a,
			.cchTextMax = 256};
		SendDlgItemMessageA(window, IDC_RAPID_AVAILABLE, TVM_GETITEMA, 0, (LPARAM)&item_info);

		if (text_b)
			sprintf(text_a + strlen(text_a), ":%s", text_b);
		else if (item_info.cChildren)
			return;
		else
			text_b = item_text2;

		char *swap;
		swap = text_a;
		text_a = text_b;
		text_b = swap;
		item = (HTREEITEM)SendDlgItemMessage(window, IDC_RAPID_AVAILABLE, TVM_GETNEXTITEM, TVGN_PARENT, (LPARAM)item);
	}
	if (download_only)
		DownloadShortMod(text_b);
	else
		SendDlgItemMessageA(window, IDC_RAPID_SELECTED, LB_ADDSTRING, 0, (LPARAM)text_b);
}

static void
rapid_on_init(HWND window)
{
#ifdef NDEBUG
	ShowWindow(GetDlgItem(window, IDC_RAPID_ERRORCHECK), 0);
	ShowWindow(GetDlgItem(window, IDC_RAPID_CLEANUP), 0);
#endif

	if (g_settings.selected_packages) {
		size_t len = strlen(g_settings.selected_packages);
		char buf[len + 1];
		buf[len] = '\0';
		char *start = buf;
		for (size_t i=0; i<len; ++i) {
			buf[i] = g_settings.selected_packages[i];
			if (buf[i] != ';')
				continue;
			buf[i] = '\0';
			SendDlgItemMessageA(window, IDC_RAPID_SELECTED, LB_ADDSTRING, 0, (LPARAM)start);
			start = buf + i + 1;
		}
		SendDlgItemMessageA(window, IDC_RAPID_SELECTED, LB_ADDSTRING, 0, (LPARAM)start);
	}

	char path[MAX_PATH];
	WIN32_FIND_DATAA find_file_data;
	char *pathEnd = path + sprintf(path, "%lsrepos\\*", g_data_dir) - 1;

	HANDLE find = FindFirstFileA(path, &find_file_data);
	do {
		if (find_file_data.cFileName[0] == '.')
			continue;
		sprintf(pathEnd, "%s\\versions.gz", find_file_data.cFileName);
		gzFile file = gzopen(path, "rb");

		char buf[1024];
		char root[1024] = {};
		HTREEITEM root_items[5] = {};

		while (gzgets(file, buf, sizeof(buf))) {
			*strchr(buf, ',') = '\0';

			size_t root_level = 0;
			int start = 0;
			int i=0;
			for (; buf[i]; ++i) {
				if (root[i] != buf[i])
					break;
				if (buf[i] == ':') {
					++root_level;
					start = i + 1;
				}
			}

			for ( ; ; ++i) {
				root[i] = buf[i];
				if (buf[i] && buf[i] != ':')
					continue;
				root[i] = '\0';

				TVINSERTSTRUCTA info;
				info.hParent = root_items[root_level];
				info.hInsertAfter = buf[i] ? TVI_SORT : 0;
				info.item.mask = TVIF_TEXT | TVIF_CHILDREN;
				info.item.pszText = &root[start];
				info.item.cChildren = !buf[i];

				root_items[++root_level] = (HTREEITEM)SendDlgItemMessageA(window, IDC_RAPID_AVAILABLE, TVM_INSERTITEMA, 0, (LPARAM)&info);

				if (!buf[i])
					break;
				root[i] = buf[i];
				start = i + 1;
			}
		}
		gzclose(file);

	} while (FindNextFileA(find, &find_file_data));
	FindClose(find);
}


static BOOL CALLBACK
rapid_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
		rapid_on_init(window);
		return 0;
	case WM_NOTIFY:
	{
		NMHDR *info = (NMHDR *)l_param;
		if (info->idFrom == IDC_RAPID_AVAILABLE
				&& (info->code == NM_DBLCLK || info->code == NM_RETURN)) {
			rapidAdd_available(window, 0);
		}
		return 0;
	}
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
		{
			HWND list_box = GetDlgItem(window, IDC_RAPID_SELECTED);
			int package_len = SendMessageA(list_box, LB_GETCOUNT, 0, 0);
			char text[1024];
			char *s = text;
			for (int i=0; i<package_len; ++i) {
				*s++ = ';';
				s += SendMessageA(list_box, LB_GETTEXT, i, (LPARAM)s);
			}
			g_settings.selected_packages = _strdup(text + 1);
			Downloader_get_selected_packages();
		}
			/* Fallthrough */
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 0;
		case MAKEWPARAM(IDC_RAPID_REMOVE, BN_CLICKED):
		{
			DWORD selected = SendDlgItemMessage(window, IDC_RAPID_SELECTED, LB_GETCURSEL, 0, 0);
			SendDlgItemMessage(window, IDC_RAPID_SELECTED, LB_DELETESTRING, selected, 0);
			return 0;
		}
		case MAKEWPARAM(IDC_RAPID_SELECT, BN_CLICKED):
			rapidAdd_available(window, 0);
			return 0;
		case MAKEWPARAM(IDC_RAPID_DOWNLOAD, BN_CLICKED):
			rapidAdd_available(window, 1);
			return 0;
		case MAKEWPARAM(IDC_RAPID_ERRORCHECK, BN_CLICKED):
			// MainWindow_msg_box(NULL, "Not implemented yet");
			return 0;
		case MAKEWPARAM(IDC_RAPID_CLEANUP, BN_CLICKED):
			// MainWindow_msg_box(NULL, "Not implemented yet");
			return 0;
		} break;
	}
	return 0;
}


void
CreateAgreementDlg(FILE *agreement)
{
	DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_AGREEMENT), g_main_window, agreement_proc, (LPARAM)agreement);
	fclose(agreement);
}

void
CreatePreferencesDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_PREFERENCES), g_main_window, preferences_proc);
}

LPARAM GetTextDlg2(HWND window, const char *title, char *buf, size_t buf_len)
{
	const char *param[] = {title, buf, (void *)buf_len};
	return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_GETTEXT), window, get_text_proc, (LPARAM)param);
}

LPARAM GetTextDlg(const char *title, char *buf, size_t buf_len)
{
	const char *param[] = {title, buf, (void *)buf_len};
	return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_GETTEXT), g_main_window, get_text_proc, (LPARAM)param);
}

void
CreateChange_password_dlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_CHANGEPASSWORD), g_main_window, change_password_proc);
}

void
CreateLoginBox(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_LOGIN), g_main_window, login_proc);
}

void
CreateHostBattleDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_HOST), g_main_window, host_proc);
}

void
CreateColorDlg(union UserOrBot *u)
{
	DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_COLOR), g_main_window, color_proc, (LPARAM)u);
}

void
CreateAboutDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_ABOUT), g_main_window, about_proc);
}

void
CreateReplayDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_REPLAY), g_main_window, replay_list_proc);
}

void
CreateRapidDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_RAPID), g_main_window, rapid_proc);
}
