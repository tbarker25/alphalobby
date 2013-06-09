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
ssize_t g_mod_option_count, g_map_option_count;
Option *g_mod_options, *g_map_options;
BattleOption g_battle_options;
const HostType *g_host_type;

uint8_t g_side_count;
char g_side_names[16][32];

char **g_maps, **g_mods;
ssize_t g_map_count = -1, g_mod_count = -1;

static char *currentScript;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

struct _LargeMapInfo _gLargeMapInfo = {.mapInfo = {.description = _gLargeMapInfo.description, .author = _gLargeMapInfo.author}};

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
	int teamBitField=0, ally=0;
	FOR_EACH_PLAYER(s, g_my_battle) {
		ally += ((s->battle_status & BS_ALLY) == 0) - ((s->battle_status & BS_ALLY) == TO_BS_ALLY_MASK(1));
		teamBitField |= 1 << FROM_BS_TEAM(s->battle_status);
	}
	int teamMask = 0;
	for (int i=0; i<16; ++i) {
		if (!(teamBitField & 1 << i)) {
			teamMask = TO_BS_TEAM_MASK(i);
			break;
		}
	}
	return BS_MODE | BS_READY | teamMask | TO_BS_ALLY_MASK(ally > 0);
}

void
MyBattle_joined_battle(Battle *restrict b, uint32_t modHash)
{
	g_my_battle = b;
	g_battle_options.modHash = modHash;

	g_my_user.battle_status = 0;
	g_last_battle_status = 0;

	if (g_mod_hash !=modHash)
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
	for (int i=0; i<LENGTH(g_battle_options.positions); ++i)
		g_battle_options.positions[i] = INVALID_STARTPOS;

	free(currentScript);
	currentScript = NULL;

	BattleRoom_on_set_mod_details();

	while (g_my_battle->nbBots)
		Users_del_bot(g_my_battle->users[g_my_battle->nbParticipants - 1]->bot.name);

	g_my_user.battle_status = 0;
	g_last_battle_status = 0;
	battleInfoFinished = 0;

	g_my_battle = NULL;
	if (g_battle_to_join)
		JoinBattle(g_battle_to_join, NULL);

	/* TODO: */
	/* *relayHoster = '\0'; */
}


void
MyBattle_update_battle_status(UserOrBot *restrict s, uint32_t bs, uint32_t color)
{
	uint32_t lastBS = s->battle_status;
	s->battle_status = bs;
	uint32_t lastColor = s->color;
	s->color = color;

	#ifdef NDEBUG
	if (!s || !g_my_battle || g_my_battle != s->battle)
		return;
	#endif

	BattleRoom_update_user(s);

	if (&s->user == &g_my_user)
		g_last_battle_status = bs;

	if ((lastBS ^ bs) & BS_MODE) {

		if (!(bs & BS_MODE)
				&& (User *)s != &g_my_user
				&& BattleRoom_is_auto_unspec())
			SetBattleStatus(&g_my_user, BS_MODE, BS_MODE);

		g_my_battle->nbSpectators = 0;
		for (int i=0; i < g_my_battle->nbParticipants; ++i)
			g_my_battle->nbSpectators += !(g_my_battle->users[i]->battle_status & BS_MODE);
		BattleList_UpdateBattle(g_my_battle);
		/* Rebalance(); */
	} else if (bs & BS_MODE
			&& ((lastBS ^ bs) & (BS_TEAM | BS_ALLY)
			   ||  lastColor != color)) {
		/* FixPlayerStatus((void *)s); */
	} else
		return;
	if ((lastBS ^ bs) & BS_MODE
			|| ((lastBS ^ bs) & BS_ALLY && &s->user == &g_my_user))
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
		for (int j=0; j < opt->nbListItems; ++j)
			AppendMenuA(menu, 0, j+1, opt->listItems[j].name);
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
		strcpy(val, opt->listItems[clicked - 1].key);
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
		StartPosType startPosType = atoi(val);
		if (startPosType != g_battle_options.startPosType) {
			;// taskSetMinimap = 1;
		}
		g_battle_options.startPosType = startPosType;
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
		for (int i=0; i<g_mod_option_count; ++i) {
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
		for (int i=0; i<g_map_option_count; ++i) {
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
	char *script = _strdup(currentScript);
	char *toFree = script;

	char *key, *val;
	while ((key = strsep(&script, "=")) && (val = strsep(&script, "\t")))
		set_option_from_tag(key, val);

	free(toFree);
}

void
MyBattle_append_script(char *restrict s)
{
	if (!currentScript) {
		currentScript = _strdup(s);

	} else {
		size_t current_len = strlen(currentScript) + 1;
		size_t extra_len = strlen(s) + 1;
		currentScript = realloc(currentScript,
				current_len + extra_len);
		currentScript[current_len - 1] = '\t';
		memcpy(currentScript + current_len, s, extra_len);
	}

	if (g_mod_hash)
		set_options_from_script();
}

void
MyBattle_update_mod_options(void)
{
	BattleRoom_on_set_mod_details();

	if (!currentScript)
		return;

	set_options_from_script();

	/* Set default values */
	for (int i=0; i<g_mod_option_count; ++i)
		if (!g_mod_options[i].val)
			BattleRoom_on_set_option(&g_mod_options[i]);

	for (int i=0; i<g_map_option_count; ++i)
		if (!g_map_options[i].val)
			BattleRoom_on_set_option(&g_map_options[i]);
}
