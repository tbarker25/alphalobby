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

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#include <windows.h>

#include "chat.h"
#include "chat_window.h"
#include "client_message.h"
#include "data.h"
#include "dialogboxes.h"
#include "sync.h"

void CreateUserMenu(union UserOrBot *s, HWND window)
{

	enum menuID {
		CHAT = 1, IGNORED, ALIAS, ID, ALLY, SIDE, COLOR, KICK,
			RING, SPEC,
		LAST=SPEC,
		FLAG_TEAM = 0x0100, FLAG_ALLY = 0x0200, FLAG_SIDE = 0x0400,
		AI_FLAG = 0x0800, AI_OPTIONS_FLAG = 0x10000,

		TEAM_MENU = 1, ALLY_MENU, SIDE_MENU, AI_MENU,
	};

	HMENU menus[100];

	int lastMenu = AI_MENU;
	for (int i=0; i<=lastMenu; ++i)
		menus[i] = CreatePopupMenu();

	uint32_t battleStatus = s->battleStatus;

	for (int i=0; i<16; ++i) {
		wchar_t buff[3];
		_swprintf(buff, L"%d", i+1);
		AppendMenu(menus[TEAM_MENU], MF_CHECKED * (i == FROM_TEAM_MASK(battleStatus)), FLAG_TEAM | i, buff);
		AppendMenu(menus[ALLY_MENU], MF_CHECKED * (i == FROM_ALLY_MASK(battleStatus)), FLAG_ALLY | i, buff);
	}

	for (int i=0; *gSideNames[i]; ++i)
		AppendMenuA(menus[SIDE_MENU], MF_CHECKED * (i == FROM_SIDE_MASK(battleStatus)), FLAG_SIDE | i, gSideNames[i]);


	if (battleStatus & AI_MASK) {
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[TEAM_MENU], L"Set ID");
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[ALLY_MENU], L"Set team");
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[SIDE_MENU], L"Set faction");
		AppendMenu(menus[0], 0, COLOR, L"Set color");
		AppendMenu(menus[0], 0, KICK, L"Remove bot");
	} else if (&s->user != &gMyUser) {
		AppendMenu(menus[0], 0, CHAT, L"Private chat");
		AppendMenu(menus[0], s->user.ignore * MF_CHECKED, IGNORED, L"Ignore");
		AppendMenu(menus[0], 0, ALIAS, L"Set alias");
		AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
		AppendMenu(menus[0], 0, RING, L"Ring");
		if (battleStatus & MODE_MASK)
			AppendMenu(menus[0], 0, SPEC, L"Force spectate");
		AppendMenu(menus[0], 0, KICK, L"Kick");
		AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[TEAM_MENU], L"Set ID");
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[ALLY_MENU], L"Set team");
	} else { //(u == &gMyUser)
		if (battleStatus & MODE_MASK) {
			AppendMenu(menus[0], 0, SPEC, L"Spectate");
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[TEAM_MENU], L"Set ID");
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[ALLY_MENU], L"Set team");
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[SIDE_MENU], L"Set faction");
			AppendMenu(menus[0], 0, COLOR, L"Set color");
		} else
			AppendMenu(menus[0], 0, SPEC, L"Unspectate");
	}
	SetMenuDefaultItem(menus[0], 1, 0);

	POINT pt;
	GetCursorPos(&pt);
	int clicked = TrackPopupMenuEx(menus[0], TPM_RETURNCMD, pt.x, pt.y, window, NULL);
	switch (clicked) {
	case 0:
		break;
	case CHAT:
		ChatWindow_SetActiveTab(GetPrivateChat(&s->user));
		break;
	case IGNORED:
		s->user.ignore ^= 1;
		/* UpdateUser(&s->user); */
		break;
	case ALIAS: {
		char title[128], buff[MAX_NAME_LENGTH_NUL];
		sprintf(title, "Set alias for %s", s->name);
		if (!GetTextDlg(title, strcpy(buff, UNTAGGED_NAME(s->name)), sizeof(buff))) {
			strcpy(s->user.alias, buff);
			/* UpdateUser(&s->user); */
		}
		} break;
	case COLOR:
		CreateColorDlg(s);
		break;
	case KICK:
		Kick(s);
		break;
	case SPEC:
		break;
	case RING:
		break;
	default:
		if (clicked & AI_FLAG) {
			// size_t len = GetMenuStringA(menus[AI_MENU], clicked, NULL, 0, MF_BYCOMMAND);
			// free(s->bot.dll);
			// s->bot.dll = malloc(len+1);
			// GetMenuStringA(menus[AI_MENU], clicked, s->bot.dll, len+1, MF_BYCOMMAND);
			// free(s->bot.options);
			// s->bot.nbOptions = UnitSync_GetSkirmishAIOptionCount(s->bot.dll);
			// s->bot.options = calloc(s->bot.nbOptions, sizeof(*s->bot.options));
			// UnitSync_GetOptions(s->bot.options, s->bot.nbOptions);
		// } else if (clicked & AI_OPTIONS_FLAG) {
			// Option2 *opt= &s->bot.options[clicked & 0xFF];
			// switch (opt->type) {
			// case opt_list:
				// strcpy(opt->val, opt->listItems[clicked >> 8 & 0xFF].key);
				// break;
			// case opt_number:
				// GetTextDlg(opt->name, opt->val, opt->val - opt->extraData + sizeof(opt->extraData));
				// break;
			// case opt_bool:
				// opt->val[0] ^= '0' ^ '1';
				// break;
			// default:
				// break;
			// }
		} else
			SetBattleStatus(s,
					(clicked & ~(FLAG_TEAM | FLAG_ALLY | FLAG_SIDE)) << (clicked & FLAG_TEAM ? TEAM_OFFSET : clicked & FLAG_ALLY ? ALLY_OFFSET : SIDE_OFFSET),
					clicked & FLAG_TEAM ? TEAM_MASK : clicked & FLAG_ALLY ? ALLY_MASK : SIDE_MASK);

		break;
	}

	for (int i=0; i<=lastMenu; ++i)
		DestroyMenu(menus[i]);
}
