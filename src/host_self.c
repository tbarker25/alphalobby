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
#include "battleroom.h"
#include "chat.h"
#include "client.h"
#include "common.h"
#include "host_self.h"
#include "mybattle.h"
#include "sync.h"
#include "user.h"

static void force_ally(const char *name, int ally);
static void force_team(const char *name, int team);
static void kick(const char *name);
static void said_battle(const char *username, char *text);
static void set_map(const char *map_name);
static void set_option(Option *opt, const char *val);
static void set_split(int size, SplitType type);

const HostType HOST_SELF = {
	.force_ally = force_ally,
	.force_team = force_team,
	.kick = kick,
	.said_battle = said_battle,
	.set_map = set_map,
	.set_option = set_option,
	.set_split = set_split,
};

static void
force_ally(const char *name, int ally)
{
	Server_send("FORCEALLYNO %s %d" , name, ally);
}

static void
force_team(const char *name, int team)
{
	Server_send("FORCETEAMNO %s %d" , name, team);
}

/* static void
force_color(const char *name, uint32_t color)    */
/* {                                                           */
/*         Server_send("FORCETEAMCOLOR %s %d", name, color); */
/* }                                                           */

static void
kick(const char *name)
{
	Server_send("KICKFROMBATTLE %s", name);
}

static void
said_battle(const char *username, char *text)
{
	Chat_said(GetBattleChat(), username, 0, text);
}

static void
set_map(const char *map_name)
{
	Server_send("UPDATEBATTLEINFO 0 0 %d %s",
			Sync_map_hash(map_name), map_name);
}

static void
set_option(Option *opt, const char *val)
{
	if (opt >= g_mod_options && opt < g_mod_options + g_mod_option_len)
		Server_send("SETSCRIPTTAGS game/modoptions/%s=%s", opt->key, val);

	else if (opt >= g_map_options && opt < g_map_options + g_map_option_len)
		Server_send("SETSCRIPTTAGS game/mapoptions/%s=%s", opt->key, val);
	else
		assert(0);
}

static void
add_start_box(int i, int left, int top, int right, int bottom)
{
	Server_send("ADDSTARTRECT %d %d %d %d %d", i, left, top, right, bottom);
}

static void
del_start_box(int i)
{
	Server_send("REMOVESTARTRECT %d", i);
}

static void
set_split(int size, SplitType type)
{
	switch (type) {
	case SPLIT_HORZ:
		add_start_box(0, 0, 0, size, 200);
		add_start_box(1, 200 - size, 0, 200, 200);
		del_start_box(2);
		del_start_box(3);
		break;
	case SPLIT_VERT:
		add_start_box(0, 0, 0, 200, size);
		add_start_box(1, 0, 200 - size, 200, 200);
		del_start_box(2);
		del_start_box(3);
		break;
	case SPLIT_CORNERS1:
		add_start_box(0, 0, 0, size, size);
		add_start_box(1, 200 - size, 200 - size, 200, 200);
		add_start_box(2, 0, 200 - size, size, 200);
		add_start_box(3, 200 - size, 0, 200, size);
		break;
	case SPLIT_CORNERS2:
		add_start_box(0, 0, 200 - size, size, 200);
		add_start_box(1, 200 - size, 0, 200, size);
		add_start_box(2, 0, 0, size, size);
		add_start_box(3, 200 - size, 200 - size, 200, 200);
		break;
	default:
		assert(0);
		break;
	}
}
