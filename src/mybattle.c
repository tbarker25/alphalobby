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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <windows.h>

#include "battle.h"
#include "battlelist.h"
#include "battleroom.h"
#include "client_message.h"
#include "common.h"
#include "dialogboxes.h"
#include "host_spads.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "settings.h"
#include "sync.h"
#include "user.h"

uint32_t g_udp_help_port;

uint32_t g_battle_to_join;

Battle *g_my_battle;
extern uint32_t g_last_battle_status;

uint32_t g_map_hash, g_mod_hash;
size_t g_mod_option_len, g_map_option_len;
Option *g_mod_options, *g_map_options;
BattleOption g_battle_options;
const HostType *g_host_type;

uint8_t g_side_len;
char g_side_names[16][32];

char **g_maps;
size_t g_map_len = 0;

char **g_mods;
size_t g_mod_len = 0;

static char *current_script;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

struct _LargeMapInfo _g_largeMapInfo = {.map_info = {.description = _g_largeMapInfo.description, .author = _g_largeMapInfo.author}};

void
MyBattle_set_split(SplitType type, int size)
{
	if (g_host_type && g_host_type->set_split)
		g_host_type->set_split(size, type);
}

void
MyBattle_set_map(const char *restrict name)
{
	if (g_host_type && g_host_type->set_map)
		g_host_type->set_map(name);
}

uint32_t
MyBattle_new_battle_status(void)
{
	int team_bit_field=0, ally=0;
	FOR_EACH_PLAYER(s, g_my_battle) {
		ally += ((s->battle_status & BS_ALLY) == 0) - ((s->battle_status & BS_ALLY) == TO_BS_ALLY_MASK(1));
		team_bit_field |= 1 << FROM_BS_TEAM(s->battle_status);
	}
	int team_mask = 0;
	for (int i=0; i<16; ++i) {
		if (!(team_bit_field & 1 << i)) {
			team_mask = TO_BS_TEAM_MASK(i);
			break;
		}
	}
	return BS_MODE | BS_READY | team_mask | TO_BS_ALLY_MASK(ally > 0);
}

void
MyBattle_joined_battle(Battle *restrict b, uint32_t mod_hash)
{
	g_my_battle = b;
	g_battle_options.mod_hash = mod_hash;

	g_my_user.battle_status = 0;
	g_last_battle_status = 0;

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

	memset(&g_battle_options, 0, sizeof(g_battle_options));
	for (size_t i=0; i<LENGTH(g_battle_options.positions); ++i)
		g_battle_options.positions[i] = INVALID_STARTPOS;

	free(current_script);
	current_script = NULL;

	BattleRoom_on_set_mod_details();

	while (g_my_battle->bot_len)
		Users_del_bot(g_my_battle->users[g_my_battle->participant_len - 1]->bot.name);

	g_my_user.battle_status = 0;
	g_last_battle_status = 0;
	g_battle_info_finished = 0;

	g_my_battle = NULL;
	if (g_battle_to_join)
		JoinBattle(g_battle_to_join, NULL);

	/* TODO: */
	/* *relay_hoster = '\0'; */
}


void
MyBattle_update_battle_status(UserOrBot *restrict s, uint32_t bs, uint32_t color)
{
	uint32_t last_b_s = s->battle_status;
	s->battle_status = bs;
	uint32_t last_color = s->color;
	s->color = color;

	#ifdef NDEBUG
	if (!s || !g_my_battle || g_my_battle != s->battle)
		return;
	#endif

	BattleRoom_update_user(s);

	if (&s->user == &g_my_user)
		g_last_battle_status = bs;

	if ((last_b_s ^ bs) & BS_MODE) {

		if (!(bs & BS_MODE)
				&& (User *)s != &g_my_user
				&& BattleRoom_is_auto_unspec())
			SetBattleStatus(&g_my_user, BS_MODE, BS_MODE);

		g_my_battle->spectator_len = 0;
		for (int i=0; i < g_my_battle->participant_len; ++i)
			g_my_battle->spectator_len += !(g_my_battle->users[i]->battle_status & BS_MODE);
		BattleList_update_battle(g_my_battle);
		/* Rebalance(); */
	} else if (bs & BS_MODE
			&& ((last_b_s ^ bs) & (BS_TEAM | BS_ALLY)
			   ||  last_color != color)) {
		/* FixPlayerStatus((void *)s); */
	} else
		return;
	if ((last_b_s ^ bs) & BS_MODE
			|| ((last_b_s ^ bs) & BS_ALLY && &s->user == &g_my_user))
		BattleRoom_on_start_position_change();
}

void
MyBattle_change_option(Option *restrict opt)
{
	char val[128];
	switch (opt->type) {
		HMENU menu;
	case opt_bool:
		val[0] = opt->val[0] == '0' ? '1' : '0';
		val[1] = '\0';
		break;
	case opt_number:
		val[0] = '\0';
		if (GetTextDlg(opt->name, val, 128))
			return;
		break;
	case opt_list:
		menu = CreatePopupMenu();
		for (size_t j=0; j < opt->list_item_len; ++j)
			AppendMenuA(menu, 0, j+1, opt->list_items[j].name);
		SetLastError(0);
		POINT point;
		GetCursorPos(&point);
		void
func(int *i) {
			*i = TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x, point.y, g_main_window, NULL);
		}
		int clicked;
		SendMessage(g_main_window, WM_EXECFUNCPARAM, (WPARAM)func, (LPARAM)&clicked);
		if (!clicked)
			return;
		strcpy(val, opt->list_items[clicked - 1].key);
		DestroyMenu(menu);
		break;
	default:
		return;
	}

	if (g_host_type && g_host_type->set_option)
		g_host_type->set_option(opt, opt->val);
}

static void
set_option_from_tag(char *restrict key, const char *restrict val)
{
	_strlwr(key);
	if (strcmp(strsep(&key, "/"), "game")) {
		printf("unrecognized script key ?/%s=%s\n", key, val);
		return;
	}

	const char *section = strsep(&key, "/");

	if (!strcmp(section, "startpostype")) {
		StartPosType start_pos_type = atoi(val);
		if (start_pos_type != g_battle_options.start_pos_type) {
			;// task_set_minimap = 1;
		}
		g_battle_options.start_pos_type = start_pos_type;
		// PostMessage(g_battle_room, WM_MOVESTARTPOSITIONS, 0, 0);
		return;
	}

#if 0
	if (!strcmp(s, "team")) {
		int team = atoi(s + sizeof("team") - 1);
		char type = s[sizeof("team/startpos") + (team > 10)];
		((int *)&g_battle_options.positions[team])[type != 'x'] = atoi(val);
		// PostMessage(g_battle_room, WM_MOVESTARTPOSITIONS, 0, 0);
		return;
	}
#endif

	if (!strcmp(section, "hosttype")) {
		if (!_stricmp(val, "spads")) {
			g_host_type = &g_host_spads;
			return;
		}
		printf("unknown hosttype %s\n", val);
		return;
	}


	if (!strcmp(section, "players")) {
		char *username = strsep(&key, "/");
		if (strcmp(key, "skill")) {
			printf("unrecognized player option %s=%s\n", key, val);
			return;
		}
		FOR_EACH_HUMAN_PLAYER(p, g_my_battle) {
			if (!_stricmp(username, p->name)) {
				free(p->skill);
				p->skill = _strdup(val);
			}
		}
		return;
	}

	if (!strcmp(section, "modoptions")) {
		for (size_t i=0; i<g_mod_option_len; ++i) {
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
		for (size_t i=0; i<g_map_option_len; ++i) {
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
	char *script = _strdup(current_script);
	char *to_free = script;

	char *key, *val;
	while ((key = strsep(&script, "=")) && (val = strsep(&script, "\t")))
		set_option_from_tag(key, val);

	free(to_free);
}

void
MyBattle_append_script(char *restrict s)
{
	if (!current_script) {
		current_script = _strdup(s);

	} else {
		size_t current_len = strlen(current_script) + 1;
		size_t extra_len = strlen(s) + 1;
		current_script = realloc(current_script,
				current_len + extra_len);
		current_script[current_len - 1] = '\t';
		memcpy(current_script + current_len, s, extra_len);
	}

	if (g_mod_hash)
		set_options_from_script();
}

void
MyBattle_update_mod_options(void)
{
	BattleRoom_on_set_mod_details();

	if (!current_script)
		return;

	set_options_from_script();

	/* Set default values */
	for (size_t i=0; i<g_mod_option_len; ++i)
		if (!g_mod_options[i].val)
			BattleRoom_on_set_option(&g_mod_options[i]);

	for (size_t i=0; i<g_map_option_len; ++i)
		if (!g_map_options[i].val)
			BattleRoom_on_set_option(&g_map_options[i]);
}
