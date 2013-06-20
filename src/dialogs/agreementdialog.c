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
#include <inttypes.h>

#include <windows.h>
#include <richedit.h>

#include "agreementdialog.h"
#include "../resource.h"
#include "../mainwindow.h"
#include "../tasserver.h"

static BOOL CALLBACK agreement_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);

void
AgreementDialog_create(FILE *agreement)
{
	DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_AGREEMENT), g_main_window,
	    agreement_proc, (LPARAM)agreement);
	fclose(agreement);
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
			TasServer_send_confirm_agreement();
			//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
		} break;
	}
	return 0;
}
