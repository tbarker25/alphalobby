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
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>

#include "battle.h"
#include "chatbox.h"
#include "chattab.h"
#include "tasserver.h"
#include "common.h"
#include "mybattle.h"
#include "sync.h"
#include "user.h"
#include "dialogs/gettextdialog.h"
#include "dialogs/colordialog.h"

void
UserMenu_spawn(UserBot *s, HWND window)
{
	enum MenuId {
		INVALID = 0,
		CHAT = 1, IGNORED, ALIAS, ID, ALLY, SIDE, COLOR, KICK,
			RING, SPEC,
		LAST=SPEC,
		FLAG_TEAM = 0x0100, FLAG_ALLY = 0x0200, FLAG_SIDE = 0x0400,
		AI_FLAG = 0x0800, AI_OPTIONS_FLAG = 0x10000,

		TEAM_MENU = 1, ALLY_MENU, SIDE_MENU, AI_MENU,
	};

	HMENU menus[100];
	enum MenuId clicked;
	POINT point;
	User *u;
	int last_menu;

	u = (User *)s;
	last_menu = AI_MENU;
	for (int i=0; i<=last_menu; ++i)
		menus[i] = CreatePopupMenu();

	for (uint8_t i=0; i<16; ++i) {
		wchar_t buf[3];

		_swprintf(buf, L"%d", i+1);
		AppendMenu(menus[TEAM_MENU], i == s->team ? MF_CHECKED : 0, (uintptr_t)FLAG_TEAM | i, buf);
		AppendMenu(menus[ALLY_MENU], i == s->ally ? MF_CHECKED : 0, (uintptr_t)FLAG_ALLY | i, buf);
	}

	for (size_t i=0; *g_side_names[i]; ++i)
		AppendMenuA(menus[SIDE_MENU], i == s->side ? MF_CHECKED : 0, FLAG_SIDE | i, g_side_names[i]);


	if (s->ai) {
		AppendMenu(menus[0], MF_POPUP, (uintptr_t)menus[TEAM_MENU], L"Set ID");
		AppendMenu(menus[0], MF_POPUP, (uintptr_t)menus[ALLY_MENU], L"Set team");
		AppendMenu(menus[0], MF_POPUP, (uintptr_t)menus[SIDE_MENU], L"Set faction");
		AppendMenu(menus[0], 0, COLOR, L"Set color");
		AppendMenu(menus[0], 0, KICK, L"Remove bot");

	} else if (s != (UserBot *)&g_my_user) {
		AppendMenu(menus[0], 0, CHAT, L"Private chat");
		AppendMenu(menus[0], u->ignore ? MF_CHECKED : 0, IGNORED, L"Ignore");
		AppendMenu(menus[0], 0, ALIAS, L"Set alias");
		AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
		AppendMenu(menus[0], 0, RING, L"Ring");
		if (s->mode)
			AppendMenu(menus[0], 0, SPEC, L"Force spectate");
		AppendMenu(menus[0], 0, KICK, L"Kick");
		AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
		AppendMenu(menus[0], MF_POPUP, (uintptr_t )menus[TEAM_MENU], L"Set ID");
		AppendMenu(menus[0], MF_POPUP, (uintptr_t )menus[ALLY_MENU], L"Set team");

	} else { //(u == &g_my_user)
		if (s->mode) {
			AppendMenu(menus[0], 0, SPEC, L"Spectate");
			AppendMenu(menus[0], MF_POPUP, (uintptr_t)menus[TEAM_MENU], L"Set ID");
			AppendMenu(menus[0], MF_POPUP, (uintptr_t )menus[ALLY_MENU], L"Set team");
			AppendMenu(menus[0], MF_POPUP, (uintptr_t )menus[SIDE_MENU], L"Set faction");
			AppendMenu(menus[0], 0, COLOR, L"Set color");

		} else {
			AppendMenu(menus[0], 0, SPEC, L"Unspectate");
		}
	}

	SetMenuDefaultItem(menus[0], 1, 0);

	GetCursorPos(&point);
	clicked = TrackPopupMenuEx(menus[0], TPM_RETURNCMD, point.x, point.y, window, NULL);

	for (int i=0; i<=last_menu; ++i)
		DestroyMenu(menus[i]);

	switch (clicked) {

	case INVALID:
		return;

	case CHAT:
		ChatTab_focus_private((User *)s);
		return;

	case IGNORED:
		u->ignore = !u->ignore;
		return;

	case ALIAS: {
		char title[128];
		char buf[MAX_NAME_LENGTH_NUL];

		sprintf(title, "Set alias for %s", s->name);
		if (!GetTextDialog_create(title, strcpy(buf, UNTAGGED_NAME(s->name)), sizeof buf))
			strcpy(u->alias, buf);
		return;
	}
	case COLOR:
		ColorDialog_create(s);
		return;

	case KICK:
		TasServer_send_kick(s);
		return;

	case SPEC:
		TasServer_send_force_spectator(u);
		return;

	case RING:
		TasServer_send_ring(u);
		return;

	default:
		/* if (clicked & AI_FLAG) { */
			// size_t len = GetMenuStringA(menus[AI_MENU], clicked, NULL, 0, MF_BYCOMMAND);
			// free(s->bot.dll);
			// s->bot.dll = malloc(len+1);
			// GetMenuStringA(menus[AI_MENU], clicked, s->bot.dll, len+1, MF_BYCOMMAND);
			// free(s->bot.options);
			// s->bot.option_len = Sync_ai_option_len(s->bot.dll);
			// s->bot.options = calloc(s->bot.option_len, sizeof *s->bot.options);
			// UnitSync_GetOptions(s->bot.options, s->bot.option_len);
		// } else if (clicked & AI_OPTIONS_FLAG) {
			// Option2 *opt= &s->bot.options[clicked & 0xFF];
			// switch (opt->type) {
			// case opt_list:
				// strcpy(opt->val, opt->list_items[clicked >> 8 & 0xFF].key);
				// break;
			// case opt_number:
				// GetTextDialog_create(opt->name, opt->val, opt->val - opt->extra_data + sizeof opt->extra_data);
				// break;
			// case opt_bool:
				// opt->val[0] ^= '0' ^ '1';
				// break;
			// default:
				// break;
			// }
		/* } else */
			/* TasServer_send_my_battle_status(s, */
					/* (clicked & ~(FLAG_TEAM | FLAG_ALLY | FLAG_SIDE)) << (clicked & FLAG_TEAM ? TEAM_OFFSET : clicked & FLAG_ALLY ? ALLY_OFFSET : SIDE_OFFSET), */
					/* clicked & FLAG_TEAM ? BS_TEAM : clicked & FLAG_ALLY ? BS_ALLY : BS_SIDE); */

		return;
	}
}
