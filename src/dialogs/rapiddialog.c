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
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include <windows.h>
#include <commctrl.h>

#include "rapiddialog.h"
#include "../resource.h"
#include "../mainwindow.h"
#include "../settings.h"

static void rapidAdd_available(HWND window, char download_only);
static void rapid_on_init(HWND window);
static BOOL CALLBACK rapid_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);

void
RapidDialog_create(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_RAPID), g_main_window,
	    (DLGPROC)rapid_proc);
}

static void
rapidAdd_available(HWND window, char __attribute__((unused)) download_only)
{
#define TVGN_NEXTSELECTED       0x000B
	HTREEITEM item = (HTREEITEM)SendDlgItemMessage(window, IDC_RAPID_AVAILABLE, TVM_GETNEXTITEM, TVGN_NEXTSELECTED, (uintptr_t)NULL);
#undef TVGN_NEXTSELECTED

	char item_text1[256];
	char item_text2[256];
	char *text_a = item_text1;
	char *text_b = NULL;
	while (item) {
		char *swap;
		TVITEMA info;

		info.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_CHILDREN;
		info.hItem = item;
		info.pszText = text_a;
		info.cchTextMax = 256;
		SendDlgItemMessageA(window, IDC_RAPID_AVAILABLE, TVM_GETITEMA, 0, (intptr_t)&info);

		if (text_b)
			sprintf(text_a + strlen(text_a), ":%s", text_b);
		else if (info.cChildren)
			return;
		else
			text_b = item_text2;

		swap = text_a;
		text_a = text_b;
		text_b = swap;
		item = (HTREEITEM)SendDlgItemMessage(window, IDC_RAPID_AVAILABLE, TVM_GETNEXTITEM, TVGN_PARENT, (intptr_t)item);
	}
	/* if (download_only) */
		/* DownloadShortMod(text_b); */
	/* else */
		/* SendDlgItemMessageA(window, IDC_RAPID_SELECTED, LB_ADDSTRING, 0, (intptr_t)text_b); */
}

static void
rapid_on_init(HWND window)
{
	char path[MAX_PATH];
	WIN32_FIND_DATAA find_file_data;
	char *path_end;
	HANDLE find;

#ifdef NDEBUG
	ShowWindow(GetDlgItem(window, IDC_RAPID_ERRORCHECK), 0);
	ShowWindow(GetDlgItem(window, IDC_RAPID_CLEANUP), 0);
#endif
	if (g_settings.selected_packages) {
		size_t len = strlen(g_settings.selected_packages);
		char buf[len + 1];
		char *start;

		buf[len] = '\0';
		start = buf;
		for (size_t i=0; i<len; ++i) {
			buf[i] = g_settings.selected_packages[i];
			if (buf[i] != ';')
				continue;
			buf[i] = '\0';
			SendDlgItemMessageA(window, IDC_RAPID_SELECTED, LB_ADDSTRING, 0, (intptr_t)start);
			start = buf + i + 1;
		}
		SendDlgItemMessageA(window, IDC_RAPID_SELECTED, LB_ADDSTRING, 0, (intptr_t)start);
	}

	path_end = path + sprintf(path, "%lsrepos\\*", g_data_dir) - 1;

	find = FindFirstFileA(path, &find_file_data);
	do {
		gzFile file;
		char buf[1024];
		char root[1024] = {0};
		HTREEITEM root_items[5] = {0};

		if (find_file_data.cFileName[0] == '.')
			continue;
		sprintf(path_end, "%s\\versions.gz", find_file_data.cFileName);
		file = gzopen(path, "rb");

		while (gzgets(file, buf, sizeof buf)) {
			size_t root_level = 0;
			int start         = 0;
			int i             = 0;

			*strchr(buf, ',') = '\0';

			for (; buf[i]; ++i) {
				if (root[i] != buf[i])
					break;
				if (buf[i] == ':') {
					++root_level;
					start = i + 1;
				}
			}

			for ( ; ; ++i) {
				TVINSERTSTRUCTA info;

				root[i] = buf[i];
				if (buf[i] && buf[i] != ':')
					continue;
				root[i] = '\0';
				info.hParent = root_items[root_level];
				info.hInsertAfter = buf[i] ? TVI_SORT : 0;
				info.item.mask = TVIF_TEXT | TVIF_CHILDREN;
				info.item.pszText = &root[start];
				info.item.cChildren = !buf[i];

				root_items[++root_level] = (HTREEITEM)SendDlgItemMessageA(window, IDC_RAPID_AVAILABLE, TVM_INSERTITEMA, 0, (intptr_t)&info);

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
rapid_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
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
			size_t package_len = (size_t)SendMessageA(list_box, LB_GETCOUNT, 0, 0);
			char text[1024];
			char *s = text;
			for (size_t i=0; i<package_len; ++i) {
				*s++ = ';';
				s += SendMessageA(list_box, LB_GETTEXT, i, (intptr_t)s);
			}
			g_settings.selected_packages = _strdup(text + 1);
			/* Downloader_get_selected_packages(); */
		}
			/* Fallthrough */
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 0;
		case MAKEWPARAM(IDC_RAPID_REMOVE, BN_CLICKED):
		{
			uint32_t selected = (uint32_t)SendDlgItemMessage(window, IDC_RAPID_SELECTED, LB_GETCURSEL, 0, 0);
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
