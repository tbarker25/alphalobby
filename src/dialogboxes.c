
#include <inttypes.h>
#include <malloc.h>
#include <stddef.h>
#include <math.h>


#include "wincommon.h"
#include <Richedit.h>

#include <windowsx.h>
#include <ctype.h>

#include "alphalobby.h"
#include "common.h"
#include "dialogboxes.h"
#include "battleroom.h"
#include "settings.h"
#include "data.h"
#include "client_message.h"
#include "spring.h"
#include "md5.h"

#include "sync.h"
#include "resource.h"

void ToMD5(char *password)
{
	if (strlen(password) != 24)
		goto doit;
	for (const char *c = password; *c; ++c)
		if (!isalnum(*c) && *c != '+' && *c != '/' && *c != '=')
			goto doit;
	return;
	doit:
	strcpy(password, GetBase64MD5sum(password, strlen(password)));
}

static int isMD5(const char *hash)
{
	if (strlen(hash) != 24)
		return 0;
	for (const char *c = hash; *c; ++c)
		if (!isalnum(*c) && *c != '+' && *c != '/' && *c != '=')
			return 0;
	return 1;
}


static void ToMD52(char *md5, const char *password)
{
	memcpy(md5, isMD5(password) ? password : GetBase64MD5sum(password, strlen(password)), BASE16_MD5_LENGTH);
	md5[BASE16_MD5_LENGTH] = 0;
}

static BOOL CALLBACK loginDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_INITDIALOG:
		SetDlgItemText(window, IDC_LOGIN_USERNAME, utf8to16(LoadSetting("username")));
		char *pw = LoadSetting("password");
		if (pw) {
			SetDlgItemText(window, IDC_LOGIN_PASSWORD, utf8to16(pw));
			SendDlgItemMessage(window, IDC_LOGIN_SAVE, BM_SETCHECK, 1, 0);
			SendDlgItemMessage(window, IDC_LOGIN_AUTOCONNECT, BM_SETCHECK, gSettings.flags & SETTING_AUTOCONNECT, 0);
		} else
			EnableWindow(GetDlgItem(window, IDC_LOGIN_AUTOCONNECT), 0);
	  return 1;
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
		case MAKEWPARAM(IDC_LOGIN_REGISTER, BN_CLICKED): {
			char username[MAX_NAME_LENGTH+1];
			if (!GetDlgItemTextA(window, IDC_LOGIN_USERNAME, username, lengthof(username))) {
				MessageBox(window, L"Enter a username less than " STRINGIFY(MAX_NAME_LENGTH) L" characters.", L"Username too long", MB_OK);
				return 1;
			}
			
			char password[BASE16_MD5_LENGTH+1];
			ToMD52(password, GetDlgItemTextA2(window, IDC_LOGIN_PASSWORD));


			if (SendDlgItemMessage(window, IDC_LOGIN_SAVE, BM_GETCHECK, 0, 0)) {
				SaveSetting("username", username);
				SaveSetting("password", password);
			}
			gSettings.flags = (gSettings.flags & ~SETTING_AUTOCONNECT) | SETTING_AUTOCONNECT * SendDlgItemMessage(window, IDC_LOGIN_AUTOCONNECT, BM_GETCHECK, 0, 0);

			if (wParam == MAKEWPARAM(IDOK, BN_CLICKED))
				Login(username, password);
			else { //wParam == (IDC_LOGIN_REGISTER, BN_CLICKED))
				char confirmPassword[sizeof(password)];
				retry:
				confirmPassword[0] = 0;
				if (GetTextDlg2(window, "Confirm password", confirmPassword, lengthof(confirmPassword)))
					break;
				ToMD5(confirmPassword);
				if (strcmp(password, confirmPassword)) {
					MessageBox(window, L"Passwords do not match.", L"Can not register new account", 0);
					goto retry;
				}
				RegisterAccount(username, password);
			}
		}	//FALLTHROUGH:
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
extern const char **gRelayManagers;
extern int gRelayManagersCount;

static BOOL CALLBACK hostDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
		case WM_INITDIALOG: {
			SetDlgItemText(window, IDC_HOST_DESCRIPTION, utf8to16(LoadSetting("last_host_description")));
			const char *def; int defIndex, i;
			HWND comboBox;
			void func(const char *name)
			{
				if (def && !strcmp(def, name))
					defIndex = i;
				SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)name);
				++i;
			}
			def = LoadSetting("last_host_mod"); i=0; defIndex=0;
			comboBox = GetDlgItem(window, IDC_HOST_MOD);
			ForEachModName(func);
			SendMessage(comboBox, CB_SETCURSEL, defIndex, 0);
			
			def = LoadSetting("last_host_map"); i=0; defIndex=0;
			comboBox = GetDlgItem(window, IDC_HOST_MAP);
			ForEachMapName(func);
			SendMessage(comboBox, CB_SETCURSEL, defIndex, 0);
			
			SetDlgItemInt(window, IDC_HOST_PORT, 8452, 0);
			SendDlgItemMessage(window, IDC_HOST_USERELAY, BM_SETCHECK, 1, 0);
			
			def = LoadSetting("last_host_manager"); defIndex=0;
			for (int i=1; i<gRelayManagersCount; ++i) {
				if (def && !strcmp(def, gRelayManagers[i]))
					defIndex = i;
				SendDlgItemMessage(window, IDC_HOST_RELAY, CB_ADDSTRING, 0, (LPARAM)utf8to16(gRelayManagers[i]));
			}
			SendDlgItemMessage(window, IDC_HOST_RELAY, CB_SETCURSEL, defIndex, 0);
			
		}	return 1;
        case WM_COMMAND:
			switch (wParam) {
                case MAKEWPARAM(IDOK, BN_CLICKED): {
					char description[128], mod[128], password[128], map[128];

					GetDlgItemTextA(window, IDC_HOST_DESCRIPTION, description, lengthof(description));
					GetDlgItemTextA(window, IDC_HOST_PASSWORD, password, lengthof(password));
					GetDlgItemTextA(window, IDC_HOST_MOD, mod, lengthof(mod));
					GetDlgItemTextA(window, IDC_HOST_MAP, map, lengthof(map));


					if (strchr(password, ' ')) {
						SetDlgItemText(window, IDC_HOST_PASSWORD, NULL);
						MessageBox(window, L"Please enter a password without spaces", L"Spaces are not allowed in passwords", 0);
						return 1;
					}
					SaveSetting("last_host_description", description);
					SaveSetting("last_host_mod", mod);
					SaveSetting("last_host_map", map);
					if (SendDlgItemMessage(window, IDC_HOST_USERELAY, BM_GETCHECK, 0, 0)) {
						char manager[128];
						GetDlgItemTextA(window, IDC_HOST_RELAY, manager, lengthof(manager));
						SaveSetting("last_host_manager", manager);
						OpenRelayBattle(description, password, mod, map, manager);
					} else {
						uint16_t port;
						port = GetDlgItemInt(window, IDC_HOST_PORT, NULL, 0);
						if (!port) {
							if (MessageBox(window, L"Use the default port? (8452)", L"Can't Proceed Without a Valid Port", MB_YESNO) == IDYES)
								port = 8452;
							else
								return 0;
						}
						OpenBattle(description, password, mod, map, port);
					}
				}	//FALLTHROUGH:
				case MAKEWPARAM(IDCANCEL, BN_CLICKED):
					EndDialog(window, 0);
					return 1;
				case MAKEWPARAM(IDC_HOST_USERELAY, BN_CLICKED): {
					int checked = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
					EnableWindow(GetDlgItem(window, IDC_HOST_RELAY), checked);
					EnableWindow(GetDlgItem(window, IDC_HOST_PORT), !checked);
					EnableWindow(GetDlgItem(window, IDC_HOST_PORT_L), !checked);
				} return 0;
			} break;
    }
	return 0;
}

static COLORREF getRGB(HWND window)
{
	return RGB(SendDlgItemMessage(window, IDC_COLOR_R_L, UDM_GETPOS, 0, 0),
					SendDlgItemMessage(window, IDC_COLOR_G_L, UDM_GETPOS, 0, 0),
					SendDlgItemMessage(window, IDC_COLOR_B_L, UDM_GETPOS, 0, 0));
}
static BOOL CALLBACK colorDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
		case WM_INITDIALOG: {
			union UserOrBot *s = (void *)lParam;
			SetDlgItemInt(window, IDC_COLOR_R, s->red, 0);
			SetDlgItemInt(window, IDC_COLOR_G, s->green, 0);
			SetDlgItemInt(window, IDC_COLOR_B, s->blue, 0);
			SendDlgItemMessage(window, IDC_COLOR_R_L, UDM_SETRANGE, 0, MAKELPARAM(255,0));
			SendDlgItemMessage(window, IDC_COLOR_G_L, UDM_SETRANGE, 0, MAKELPARAM(255,0));
			SendDlgItemMessage(window, IDC_COLOR_B_L, UDM_SETRANGE, 0, MAKELPARAM(255,0));
			SetWindowLongPtr(window, GWLP_USERDATA, lParam);
			} return 1;
        case WM_COMMAND:
			switch (wParam) {
			case MAKEWPARAM(IDC_COLOR_R, EN_CHANGE):
			case MAKEWPARAM(IDC_COLOR_G, EN_CHANGE):
			case MAKEWPARAM(IDC_COLOR_B, EN_CHANGE): {
				BOOL a;
				if (GetDlgItemInt(window, LOWORD(wParam), &a, 0)  > 255)
					SetDlgItemInt(window, LOWORD(wParam), 255, 0);
				else if (!a)
					SetDlgItemInt(window, LOWORD(wParam), 0, 0);
				else if (GetWindowLongPtr(window, GWLP_USERDATA)) //Make sure WM_INITDIALOG has been called so IDC_COLOR_PREVIEW actually exists
					InvalidateRect(GetDlgItem(window, IDC_COLOR_PREVIEW), NULL, 0);
			} return 0;
			case MAKEWPARAM(IDOK, BN_CLICKED): {
				union UserOrBot *s = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
				uint32_t color = getRGB(window);
				SetColor(s, color);
				} // fallthrough:
			case MAKEWPARAM(IDCANCEL, BN_CLICKED):
				EndDialog(window, 0);
				return 0;
			}
			break;
		case WM_CTLCOLORSTATIC:
			return (GetDlgCtrlID((HWND)lParam) == IDC_COLOR_PREVIEW) * (INT_PTR)CreateSolidBrush(getRGB(window));
    }
	return 0;
}

static BOOL CALLBACK getTextDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_INITDIALOG: {
		const char **s = (void *)lParam;
		SetWindowTextA(window, s[0]);
		HWND textbox = GetDlgItem(window, IDC_GETTEXT);
		SetWindowTextA(textbox, s[1]);
		SetWindowLongPtr(textbox, GWLP_USERDATA, (LPARAM)s[2]);
		SetWindowLongPtr(window, GWLP_USERDATA, (LPARAM)s[1]);
		} break;
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED): {
			char *buff = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
			GetDlgItemTextA(window, IDC_GETTEXT, buff, GetWindowLongPtr(GetDlgItem(window, IDC_GETTEXT), GWLP_USERDATA));
			EndDialog(window, 0);
			} return 1;
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 1);
			return 1;
		} break;
    }
	return 0;
}


static BOOL CALLBACK changePasswordDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED): {
			char oldPassword[BASE16_MD5_LENGTH+1], newPassword[BASE16_MD5_LENGTH+1], confirmPassword[BASE16_MD5_LENGTH+1];
			ToMD52(GetDlgItemTextA2(window, IDC_PASSWORD_OLD), oldPassword);
			ToMD52(GetDlgItemTextA2(window, IDC_PASSWORD_NEW), newPassword);
			ToMD52(GetDlgItemTextA2(window, IDC_PASSWORD_CONFIRM), confirmPassword);

			if (strcmp(newPassword, confirmPassword))
				MessageBox(window, L"Passwords don't match", L"Couldn't Change Password", 0);
			else {
				ChangePassword(oldPassword, newPassword);
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

static BOOL CALLBACK preferencesDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_INITDIALOG: {
		HWND autoconnect = GetDlgItem(window, IDC_PREFERENCES_AUTOCONNECT);
		SendMessage(autoconnect, BM_SETCHECK, gSettings.flags & SETTING_AUTOCONNECT, 0);
		ShowWindow(autoconnect, !!LoadSetting("password"));
		SetDlgItemTextA(window, IDC_PREFERENCES_PATH, gSettings.spring_path);
		return 0;
	}
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED): {
			char buff[1024];
			GetDlgItemTextA(window, IDC_PREFERENCES_PATH, buff, sizeof(buff));
			free(gSettings.spring_path);
			gSettings.spring_path = strdup(buff);
			gSettings.flags = (gSettings.flags & ~SETTING_AUTOCONNECT) | SETTING_AUTOCONNECT * SendDlgItemMessage(window, IDC_PREFERENCES_AUTOCONNECT, BM_GETCHECK, 0, 0);
		}	//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED): 
			EndDialog(window, 0);
			return 1;

		case MAKEWPARAM(IDC_PREFERENCES_RESET, BN_CLICKED): 
			if (IDYES == MessageBox(window, L"This action cannot be reversed.", L"Reset Preferences", MB_YESNO)) {
				ResetSettings();
				SendMessage(window, WM_INITDIALOG, 0, 0);
			}
			return 1;
		} break;
    }
	return 0;
}

static BOOL CALLBACK agreementDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_INITDIALOG: {
		DWORD CALLBACK editStreamCallback(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb)
		{
			*pcb = fread(lpBuff, 1, cb, (FILE *)lParam);
			return 0;
		}

		SendDlgItemMessage(window, IDC_AGREEMENT_TEXT, EM_STREAMIN, SF_RTF, (LPARAM)&(EDITSTREAM){.pfnCallback = editStreamCallback});
		break;
	}
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			ConfirmAgreement();
			//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
		} break;
    }
	return 0;
}

static BOOL CALLBACK aboutDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_INITDIALOG: {
		HWND editBox = GetDlgItem(window, IDC_ABOUT_EDIT);
		SendMessage(editBox, EM_AUTOURLDETECT, 1, 0);
		SendMessage(editBox, EM_SETEVENTMASK, 0, ENM_LINK | ENM_MOUSEEVENTS | ENM_SCROLL);
		SendMessage(editBox, EM_SETTEXTEX, (WPARAM)&(SETTEXTEX){},
			(LPARAM)"{\\rtf {\\b Alphalobby} {\\i" STRINGIFY(VERSION) "}\\par\\par "
			"{\\b Author:}\\par Tom Barker (Axiomatic)\\par \\par "
			"{\\b Homepage:}\\par http://code.google.com/p/alphalobby/\\par \\par "
			"{\\b Flag icons:}\\par Mark James http://www.famfamfam.com/\\par }");
	} return 0;
	case WM_NOTIFY: {
		ENLINK *s = (void *)lParam;
		if (s->nmhdr.idFrom == IDC_ABOUT_EDIT && s->nmhdr.code == EN_LINK && s->msg == WM_LBUTTONUP) {
			SendMessage(s->nmhdr.hwndFrom, EM_EXSETSEL, 0, (LPARAM)&s->chrg);
			wchar_t url[256];
			SendMessage(s->nmhdr.hwndFrom, EM_GETTEXTEX,
					(WPARAM)&(GETTEXTEX){.cb = sizeof(url), .flags = GT_SELECTION, .codepage = 1200},
					(LPARAM)url);
			ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
		}
	} return 0;
	case WM_COMMAND:
		if (wParam == MAKEWPARAM(IDCANCEL, BN_CLICKED))
			EndDialog(window, 0);
		return 0;
    }
	return 0;
}

static void activateReplayListItem(HWND listViewWindow, int index)
{
	printf("index = %d\n", index);
	wchar_t buff[1024];
	LVITEM item = {.pszText = buff, lengthof(buff)};
	SendMessage(listViewWindow, LVM_GETITEMTEXT, index, (LPARAM)&item);
	wprintf(L"buff = %s\n", buff);
	LaunchReplay(buff);
}

static BOOL CALLBACK replayListProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_INITDIALOG: {
		HWND listWindow = GetDlgItem(window, IDC_REPLAY_LIST);
		SendMessage(listWindow, LVM_INSERTCOLUMN, 0,
			(LPARAM)&(LVCOLUMN){LVCF_TEXT, .pszText = L"filename"});
		SendDlgItemMessage(window, IDC_REPLAY_LIST, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
		UnitSync_AddReplaysToListView(listWindow);
		SendDlgItemMessage(window, IDC_REPLAY_LIST, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
		return 0;
	}
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED): {
			HWND listWindow = GetDlgItem(window, IDC_REPLAY_LIST);
			activateReplayListItem(listWindow, SendMessage(listWindow, LVM_GETNEXTITEM, -1, LVNI_SELECTED));
		}	//FALLTHROUGH:
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			EndDialog(window, 0);
			return 0;
		} break;
	case WM_NOTIFY: {
		if (((NMHDR *)lParam)->code == LVN_ITEMACTIVATE) {
			activateReplayListItem(((NMITEMACTIVATE *)lParam)->hdr.hwndFrom, ((NMITEMACTIVATE *)lParam)->iItem);
			EndDialog(window, 0);
		}
	} break;
    }
	return 0;
}

void CreateAgreementDlg(FILE *agreement)
{
	DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_AGREEMENT), gMainWindow, agreementDlgProc, (LPARAM)agreement);
	fclose(agreement);
}

void CreatePreferencesDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_PREFERENCES), gMainWindow, preferencesDlgProc);
}

LPARAM GetTextDlg2(HWND window, const char *title, char *buff, size_t buffLen)
{
	return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_GETTEXT), window, getTextDlgProc, (LPARAM)(const char *[]){title, buff, (void *)buffLen});
}

LPARAM GetTextDlg(const char *title, char *buff, size_t buffLen)
{
	return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_GETTEXT), gMainWindow, getTextDlgProc, (LPARAM)(const char *[]){title, buff, (void *)buffLen});
}

void CreateChangePasswordDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_CHANGEPASSWORD), gMainWindow, changePasswordDlgProc);
}

void CreateLoginBox(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_LOGIN), gMainWindow, loginDlgProc);
}

void CreateHostBattleDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_HOST), gMainWindow, hostDlgProc);
}

void CreateColorDlg(union UserOrBot *u)
{
	DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_COLOR), gMainWindow, colorDlgProc, (LPARAM)u);
}

void CreateAboutDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_ABOUT), gMainWindow, aboutDlgProc);
	// MessageBox(gMainWindow, L"Created by Axiomatic\nhttp://springrts.com/phpbb/viewtopic.php?f=64&p=469673\nFlag icons by Mark James (http://www.famfamfam.com/)\n", L"Alphalobby " STRINGIFY(VERSION), 0);
}

void CreateReplayDlg(void)
{
	DialogBox(NULL, MAKEINTRESOURCE(IDD_REPLAY), gMainWindow, replayListProc);
}


