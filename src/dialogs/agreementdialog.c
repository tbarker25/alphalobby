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
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>
#include <richedit.h>

#include "agreementdialog.h"
#include "../resource.h"
#include "../mainwindow.h"
#include "../tasserver.h"

static BOOL CALLBACK agreement_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);

void
AgreementDialog_create(FILE *agreement)
{
	DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_AGREEMENT), g_main_window,
	    (DLGPROC)agreement_proc, (intptr_t)agreement);
	fclose(agreement);
}

static BOOL CALLBACK
agreement_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		uint32_t CALLBACK edit_stream_callback(
				__attribute__((unused)) uintptr_t dwCookie,
				uint8_t * buff, int32_t cb, int32_t * pcb)
		{
			*pcb = (int32_t)fread(buff, 1, (size_t)cb, (FILE *)l_param);
			return 0;
		}

		SendDlgItemMessage(window, IDC_AGREEMENT_TEXT, EM_STREAMIN, SF_RTF, (intptr_t)&(EDITSTREAM){.pfnCallback = (void *)edit_stream_callback});
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
