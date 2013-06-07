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

#include "battleroom.h"
#include "battletools.h"
#include "chat.h"
#include "client_message.h"
#include "data.h"
#include "host_self.h"
#include "sync.h"

static void forceAlly(const char *name, int allyId);
static void forceTeam(const char *name, int teamId);
static void kick(const char *name);
static void saidBattle(const char *userName, char *text);
static void setMap(const char *mapName);
static void setOption(Option *opt, const char *val);
static void setSplit(int size, SplitType type);

const HostType gHostSelf = {
	.forceAlly = forceAlly,
	.forceTeam = forceTeam,
	.kick = kick,
	.saidBattle = saidBattle,
	.setMap = setMap,
	.setOption = setOption,
	.setSplit = setSplit,
};

static void forceAlly(const char *name, int allyId)
{
	SendToServer("FORCEALLYNO %s %d" , name, allyId);
}

static void forceTeam(const char *name, int teamId)
{
	SendToServer("FORCETEAMNO %s %d" , name, teamId);
}

/* static void forceColor(const char *name, uint32_t color)    */
/* {                                                           */
/*         SendToServer("FORCETEAMCOLOR %s %d", name, color); */
/* }                                                           */

static void kick(const char *name)
{
	SendToServer("KICKFROMBATTLE %s", name);
}

static void saidBattle(const char *userName, char *text)
{
	Chat_Said(GetBattleChat(), userName, 0, text);
}

static void setMap(const char *mapName)
{
	SendToServer("UPDATEBATTLEINFO 0 0 %d %s",
			GetMapHash(mapName), mapName);
}

static void setOption(Option *opt, const char *val)
{
	const char *path;
	if (opt >= gModOptions && opt < gModOptions + gNbModOptions)
		path = "modoptions/";
	else if (opt >= gMapOptions && opt < gMapOptions + gNbMapOptions)
		path = "mapoptions/";
	else
		assert(0);

	SendToServer("SETSCRIPTTAGS game/%s%s=%s", path ?: "", opt->key, opt->val);
}

static void addStartBox(int i, int left, int top, int right, int bottom)
{
	SendToServer("ADDSTARTRECT %d %d %d %d %d", i, left, top, right, bottom);
}

static void delStartBox(int i)
{
	SendToServer("REMOVESTARTRECT %d", i);
}

static void setSplit(int size, SplitType type)
{
	switch (type) {
	case SPLIT_HORZ:
		addStartBox(0, 0, 0, size, 200);
		addStartBox(1, 200 - size, 0, 200, 200);
		delStartBox(2);
		delStartBox(3);
		break;
	case SPLIT_VERT:
		addStartBox(0, 0, 0, 200, size);
		addStartBox(1, 0, 200 - size, 200, 200);
		delStartBox(2);
		delStartBox(3);
		break;
	case SPLIT_CORNERS1:
		addStartBox(0, 0, 0, size, size);
		addStartBox(1, 200 - size, 200 - size, 200, 200);
		addStartBox(2, 0, 200 - size, size, 200);
		addStartBox(3, 200 - size, 0, 200, size);
		break;
	case SPLIT_CORNERS2:
		addStartBox(0, 0, 200 - size, size, 200);
		addStartBox(1, 200 - size, 0, 200, size);
		addStartBox(2, 0, 0, size, size);
		addStartBox(3, 200 - size, 200 - size, 200, 200);
		break;
	default:
		assert(0);
		break;
	}
}
