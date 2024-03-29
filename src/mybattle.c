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
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <windows.h>

#include "battle.h"
#include "battlelist.h"
#include "battleroom.h"
#include "chatbox.h"
#include "common.h"
#include "dialogs/gettextdialog.h"
#include "host_spads.h"
#include "mainwindow.h"
#include "minimap.h"
#include "mybattle.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "tasserver.h"
#include "user.h"

static void set_options_from_script(void);
static void set_option_from_tag(char *restrict key, const char *restrict val);

uint32_t g_udp_help_port;
uint32_t g_battle_to_join;

Battle *g_my_battle;
bool g_battle_info_finished;

uint32_t g_map_hash, g_mod_hash;
int g_mod_option_len, g_map_option_len;
Option *g_mod_options, *g_map_options;
BattleOption g_battle_options;
uint32_t g_last_auto_unspec;

uint8_t g_side_len;
char g_side_names[16][32];

char **g_maps;
int g_map_len = 0;

char **g_mods;
int g_mod_len = 0;

bool g_is_using_trueskill;

static char *script;

void (*MyBattle_force_ally) (const User *, int ally_id);
void (*MyBattle_force_team) (const User *, int team_id);
void (*MyBattle_kick)       (const UserBot *);
void (*MyBattle_said_battle)(User *, char *text, ChatType);
void (*MyBattle_set_map)    (const char *map_name);
void (*MyBattle_set_option) (Option *opt, const char *val);
void (*MyBattle_set_split)  (int size, SplitType type);
void (*MyBattle_start_game) (void);

#define LENGTH(x) (sizeof x / sizeof *x)

MapInfo_ g_map_info = {.MapInfo = {.description = g_map_info._description, .author = g_map_info._author}};

static void
do_nothing(void)
{
}

void
MyBattle_reset(void)
{
	MyBattle_force_ally  = (void *)do_nothing;
	MyBattle_force_team  = (void *)do_nothing;
	MyBattle_kick        = (void *)do_nothing;
	MyBattle_said_battle = (void *)BattleRoom_said_battle;
	MyBattle_set_map     = (void *)do_nothing;
	MyBattle_set_option  = (void *)do_nothing;
	MyBattle_set_split   = (void *)do_nothing;
	MyBattle_start_game  = Spring_launch;
}

BattleStatus
MyBattle_new_battle_status(void)
{
	BattleStatus ret = {0};
	uint16_t team_bit_field;
	int ally_diff;

	ret.mode = 1;
	ret.ready = 1;

	team_bit_field = 0;
	ally_diff = 0;

	Battles_for_each_user(u, g_my_battle) {
		if (!u->mode)
			break;
		ally_diff += u->ally == 0;
		ally_diff -= u->ally == 1;
		team_bit_field |= (uint16_t)(1 << u->team);
	}

	ret.ally = ally_diff < 0;

	for (uint8_t i=0; i<16; ++i) {
		if (~team_bit_field & 1 << i) {
			ret.team = i & 0xfu;
			break;
		}
	}

	return ret;
}

void
MyBattle_joined_battle(Battle *restrict b, uint32_t mod_hash)
{
	g_my_battle = b;
	g_battle_options.mod_hash = mod_hash;
	MyBattle_reset();

	g_my_user.BattleStatus = (BattleStatus){0};
	g_last_battle_status = (BattleStatus){0};

	if (g_mod_hash !=mod_hash)
		Sync_on_changed_mod(b->mod_name);
	if (g_map_hash != b->map_hash || !g_map_hash)
		Sync_on_changed_map(b->map_name);

	BattleRoom_show();
}

void
MyBattle_left_battle(void)
{
	BattleRoom_hide();

	memset(&g_battle_options, 0, sizeof g_battle_options);
	for (size_t i=0; i<LENGTH(g_battle_options.positions); ++i)
		g_battle_options.positions[i] = INVALID_STARTPOS;

	free(script);
	script = NULL;

	BattleRoom_on_set_mod_details();

	/* while (g_my_battle->first_bot) */
		/* Users_del_bot(g_my_battle->first_bot->name); */

	g_my_user.BattleStatus = (BattleStatus){0};
	g_last_battle_status = (BattleStatus){0};
	g_battle_info_finished = 0;
	g_is_using_trueskill = 0;

	g_my_battle = NULL;
	if (g_battle_to_join)
		TasServer_send_join_battle(g_battle_to_join, NULL);

	/* TODO: */
	/* *relay_hoster = '\0'; */
}


void
MyBattle_update_battle_status(UserBot *restrict u, BattleStatus bs, uint32_t color)
{
	BattleStatus last_bs;

	last_bs = u->BattleStatus;
	u->BattleStatus = bs;
	u->color = color;

#ifdef NDEBUG
	if (!u || !g_my_battle || g_my_battle != u->battle)
		return;
#endif

	BattleRoom_update_user(u);

	if (u == &g_my_user.UserBot)
		g_last_battle_status = bs;

	if (last_bs.mode != bs.mode
	    || (last_bs.ally != bs.ally && u == &g_my_user.UserBot))
		Minimap_on_start_position_change();

	if (last_bs.mode == bs.mode)
		return;

	if (!bs.mode && !g_my_user.mode && u != &g_my_user.UserBot
	    && g_last_auto_unspec + 5000 < GetTickCount()
	    && BattleRoom_is_auto_unspec()) {
		BattleStatus new_bs;

		g_last_auto_unspec = GetTickCount();
		new_bs = g_last_battle_status;
		new_bs.mode = 1;
		TasServer_send_my_battle_status(new_bs);
	}

	BattleList_update_battle(g_my_battle);
}

void
MyBattle_change_option(Option *restrict opt)
{
	char val[128];

	switch (opt->type) {
	case opt_bool:
		val[0] = opt->val[0] == '0' ? '1' : '0';
		val[1] = '\0';
		break;

	case opt_number:
		val[0] = '\0';
		if (GetTextDialog_create(opt->name, val, 128))
			return;
		break;

	case opt_list: {
		int clicked;
		HMENU menu;
		POINT point;
		void func(int *i) {
			*i = TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x,
			    point.y, g_main_window, NULL);
		}

		menu = CreatePopupMenu();
		for (int j=0; j < opt->list_item_len; ++j)
			AppendMenuA(menu, 0, (uintptr_t)j + 1, opt->list_items[j].name);
		SetLastError(0);
		GetCursorPos(&point);
		SendMessage(g_main_window, WM_EXECFUNCPARAM, (uintptr_t)func, (intptr_t)&clicked);
		DestroyMenu(menu);
		if (!clicked)
			return;
		strcpy(val, opt->list_items[clicked - 1].key);
		break;
	}
	default:
		return;
	}

	MyBattle_set_option(opt, opt->val);
}

static void
set_option_from_tag(char *restrict key, const char *restrict val)
{
	const char *section;

	if (strcmp(strsep(&key, "/"), "game")) {
		printf("unrecognized script key ?/%s=%s\n", key, val);
		return;
	}

	section = strsep(&key, "/");

	if (!strcmp(section, "startpostype")) {
		StartPosType start_pos_type;

		start_pos_type = atoi(val);
		if (start_pos_type != g_battle_options.start_pos_type) {
			;// task_set_minimap = 1;
		}
		g_battle_options.start_pos_type = start_pos_type;
		// PostMessage(battle_room, WM_MOVESTARTPOSITIONS, 0, 0);
		return;
	}

#if 0
	if (!strcmp(s, "team")) {
		int team = atoi(s + sizeof "team" - 1);
		char type = s[sizeof "team/startpos" + (team > 10)];
		((int *)&g_battle_options.positions[team])[type != 'x'] = atoi(val);
		// PostMessage(battle_room, WM_MOVESTARTPOSITIONS, 0, 0);
		return;
	}
#endif

	if (!strcmp(section, "hosttype")) {
		if (!_stricmp(val, "spads")) {
			Spads_set_as_host();
			return;
		}
		printf("unknown hosttype %s\n", val);
		return;
	}


	if (!strcmp(section, "players")) {
		char *username;

		username = strsep(&key, "/");
		if (strcmp(key, "skill")) {
			printf("unrecognized player option %s=%s\n", key, val);
			return;
		}
		Battles_for_each_human(u, g_my_battle) {
			int skill;

			if (_stricmp(u->name, username))
				continue;

			while (!isdigit(*val))
				++val;
			skill = atoi(val);
			if (skill < 0 || skill > UINT8_MAX) {
				assert(0);
				return;
			}
			u->trueskill = (uint8_t)atoi(val);
			g_is_using_trueskill = 1;
			BattleRoom_update_user(u);
			return;
		}
		return;
	}

	if (!strcmp(section, "modoptions")) {
		for (int i=0; i<g_mod_option_len; ++i) {
			if (strcmp(g_mod_options[i].key, key))
				continue;
			free(g_mod_options[i].val);
			g_mod_options[i].val = _strdup(val);
			BattleRoom_on_set_option(&g_mod_options[i]);
			return;
		}
		printf("unrecognized mod option %s=%s\n", key, val);
		return;
	}

	if (!strcmp(section, "mapoptions")) {
		for (int i=0; i<g_map_option_len; ++i) {
			if (strcmp(g_map_options[i].key, key))
				continue;
			free(g_map_options[i].val);
			g_map_options[i].val = _strdup(val);
			BattleRoom_on_set_option(&g_map_options[i]);
			return;
		}
		printf("unrecognized map option %s=%s\n", key, val);
		return;
	}

	printf("unrecognized script game/%s/%s=%s\n", section, key, val);
}

static void
set_options_from_script(void)
{
	char *tmp_script;
	char *s;
	char *key, *val;

	tmp_script = _strdup(script);
	s = tmp_script;

	while ((key = strsep(&s, "=")) && (val = strsep(&s, "\t")))
		set_option_from_tag(key, val);

	free(tmp_script);
}

void
MyBattle_append_script(char *restrict s)
{
	if (!script) {
		script = _strdup(s);

	} else {
		size_t current_len = strlen(script) + 1;
		size_t extra_len = strlen(s) + 1;
		script = realloc(script,
				current_len + extra_len);
		script[current_len - 1] = '\t';
		memcpy(script + current_len, s, extra_len);
	}

	if (g_mod_hash)
		set_options_from_script();
}

void
MyBattle_update_mod_options(void)
{
	BattleRoom_on_set_mod_details();

	if (!script)
		return;

	set_options_from_script();

	/* Set default values */
	for (int i=0; i<g_mod_option_len; ++i)
		if (!g_mod_options[i].val)
			BattleRoom_on_set_option(&g_mod_options[i]);

	for (int i=0; i<g_map_option_len; ++i)
		if (!g_map_options[i].val)
			BattleRoom_on_set_option(&g_map_options[i]);
}
