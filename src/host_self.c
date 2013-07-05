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
#include "chatbox.h"
#include "tasserver.h"
#include "common.h"
#include "host_self.h"
#include "mybattle.h"
#include "sync.h"
#include "user.h"

static void set_option (Option *opt, const char *val);
static void set_split  (int size,    SplitType type);

void
Self_set_as_host(void)
{
	MyBattle_force_ally  = (void *)TasServer_send_force_ally;
	MyBattle_force_team  = (void *)TasServer_send_force_team;
	MyBattle_kick        = TasServer_send_kick;
	MyBattle_set_map     = TasServer_send_set_map;
	MyBattle_set_option  = set_option;
	MyBattle_set_split   = set_split;
}

static void
set_option(Option *opt, const char *val)
{
	if (opt >= g_mod_options && opt < g_mod_options + g_mod_option_len) {
		char buf[128];
		sprintf(buf, "game/modoptions/%s=%s", opt->key, val);
		TasServer_send_script_tags(buf);

	} else if (opt >= g_map_options && opt < g_map_options + g_map_option_len) {
		char buf[128];
		sprintf(buf, "game/mapoptions/%s=%s", opt->key, val);
		TasServer_send_script_tags(buf);

	} else {
		assert(0);
	}
}

static void
set_split(int size, SplitType type)
{
	switch (type) {
	case SPLIT_HORZ:
		TasServer_send_add_start_box(0, 0, 0, size, 200);
		TasServer_send_add_start_box(1, 200 - size, 0, 200, 200);
		TasServer_send_del_start_box(2);
		TasServer_send_del_start_box(3);
		return;

	case SPLIT_VERT:
		TasServer_send_add_start_box(0, 0, 0, 200, size);
		TasServer_send_add_start_box(1, 0, 200 - size, 200, 200);
		TasServer_send_del_start_box(2);
		TasServer_send_del_start_box(3);
		return;

	case SPLIT_CORNERS1:
		TasServer_send_add_start_box(0, 0, 0, size, size);
		TasServer_send_add_start_box(1, 200 - size, 200 - size, 200, 200);
		TasServer_send_add_start_box(2, 0, 200 - size, size, 200);
		TasServer_send_add_start_box(3, 200 - size, 0, 200, size);
		return;

	case SPLIT_CORNERS2:
		TasServer_send_add_start_box(0, 0, 200 - size, size, 200);
		TasServer_send_add_start_box(1, 200 - size, 0, 200, size);
		TasServer_send_add_start_box(2, 0, 0, size, size);
		TasServer_send_add_start_box(3, 200 - size, 200 - size, 200, 200);
		return;

	default:
		assert(0);
		return;
	}
}
