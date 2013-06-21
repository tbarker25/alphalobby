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

void
UserMenu_spawn(UserBot *s, HWND window)
{
	printf("%p %p\n", s, window);
#if 0
	enum menuID {
		CHAT = 1, IGNORED, ALIAS, ID, ALLY, SIDE, COLOR, KICK,
			RING, SPEC,
		LAST=SPEC,
		FLAG_TEAM = 0x0100, FLAG_ALLY = 0x0200, FLAG_SIDE = 0x0400,
		AI_FLAG = 0x0800, AI_OPTIONS_FLAG = 0x10000,

		TEAM_MENU = 1, ALLY_MENU, SIDE_MENU, AI_MENU,
	};

	HMENU menus[100];

	int last_menu = AI_MENU;
	for (int i=0; i<=last_menu; ++i)
		menus[i] = CreatePopupMenu();

	uint32_t battle_status = s->battle_status;

	for (uint8_t i=0; i<16; ++i) {
		wchar_t buf[3];
		_swprintf(buf, L"%d", i+1);
		AppendMenu(menus[TEAM_MENU], i == s->team ? MF_CHECKED : 0, FLAG_TEAM | i, buf);
		AppendMenu(menus[ALLY_MENU], i == s->ally ? MF_CHECKED : 0, FLAG_ALLY | i, buf);
	}

	for (size_t i=0; *g_side_names[i]; ++i)
		AppendMenuA(menus[SIDE_MENU], i == s->side ? MF_CHECKED : 0, FLAG_SIDE | i, g_side_names[i]);


	if (battle_status & BS_AI) {
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[TEAM_MENU], L"Set ID");
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[ALLY_MENU], L"Set team");
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[SIDE_MENU], L"Set faction");
		AppendMenu(menus[0], 0, COLOR, L"Set color");
		AppendMenu(menus[0], 0, KICK, L"Remove bot");
	} else if (&s->user != &g_my_user) {
		AppendMenu(menus[0], 0, CHAT, L"Private chat");
		AppendMenu(menus[0], s->user.ignore * MF_CHECKED, IGNORED, L"Ignore");
		AppendMenu(menus[0], 0, ALIAS, L"Set alias");
		AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
		AppendMenu(menus[0], 0, RING, L"MainWindow_ring");
		if (battle_status & BS_MODE)
			AppendMenu(menus[0], 0, SPEC, L"Force spectate");
		AppendMenu(menus[0], 0, KICK, L"Kick");
		AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[TEAM_MENU], L"Set ID");
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[ALLY_MENU], L"Set team");
	} else { //(u == &g_my_user)
		if (battle_status & BS_MODE) {
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
		ChatWindow_set_active_tab(Chat_get_private_window(&s->user));
		break;
	case IGNORED:
		s->user.ignore ^= 1;
		/* UpdateUser(&s->user); */
		break;
	case ALIAS: {
		char title[128], buf[MAX_NAME_LENGTH_NUL];
		sprintf(title, "Set alias for %s", s->name);
		if (!GetTextDialog_create(title, strcpy(buf, UNTAGGED_NAME(s->name)), sizeof(buf))) {
			strcpy(s->user.alias, buf);
			/* UpdateUser(&s->user); */
		}
		} break;
	case COLOR:
		CreateColorDlg(s);
		break;
	case KICK:
		TasServer_send_kick(s);
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
			// s->bot.option_len = Sync_ai_option_len(s->bot.dll);
			// s->bot.options = calloc(s->bot.option_len, sizeof(*s->bot.options));
			// UnitSync_GetOptions(s->bot.options, s->bot.option_len);
		// } else if (clicked & AI_OPTIONS_FLAG) {
			// Option2 *opt= &s->bot.options[clicked & 0xFF];
			// switch (opt->type) {
			// case opt_list:
				// strcpy(opt->val, opt->list_items[clicked >> 8 & 0xFF].key);
				// break;
			// case opt_number:
				// GetTextDialog_create(opt->name, opt->val, opt->val - opt->extra_data + sizeof(opt->extra_data));
				// break;
			// case opt_bool:
				// opt->val[0] ^= '0' ^ '1';
				// break;
			// default:
				// break;
			// }
		} else
			/* TasServer_send_my_battle_status(s, */
					/* (clicked & ~(FLAG_TEAM | FLAG_ALLY | FLAG_SIDE)) << (clicked & FLAG_TEAM ? TEAM_OFFSET : clicked & FLAG_ALLY ? ALLY_OFFSET : SIDE_OFFSET), */
					/* clicked & FLAG_TEAM ? BS_TEAM : clicked & FLAG_ALLY ? BS_ALLY : BS_SIDE); */

		break;
	}

	for (int i=0; i<=last_menu; ++i)
		DestroyMenu(menus[i]);
#endif
}
