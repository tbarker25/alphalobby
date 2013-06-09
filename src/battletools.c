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

#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>

#include <windows.h>

#include "alphalobby.h"
#include "battlelist.h"
#include "battleroom.h"
#include "battletools.h"
#include "client_message.h"
#include "data.h"
#include "dialogboxes.h"
#include "settings.h"
#include "host_spads.h"
#include "sync.h"

uint32_t gUdpHelpPort;

uint32_t battleToJoin;

Battle *gMyBattle;
extern uint32_t gLastBattleStatus;

uint32_t gMapHash, gModHash;
ssize_t gNbModOptions, gNbMapOptions;
Option *gModOptions, *gMapOptions;
BattleOption gBattleOptions;
const HostType *gHostType;

uint8_t gNbSides;
char gSideNames[16][32];

char **gMaps, **gMods;
ssize_t gNbMaps = -1, gNbMods = -1;

static char *currentScript;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

struct _LargeMapInfo _gLargeMapInfo = {.mapInfo = {.description = _gLargeMapInfo.description, .author = _gLargeMapInfo.author}};


void SetSplit(SplitType type, int size)
{
	if (gHostType && gHostType->setSplit)
		gHostType->setSplit(size, type);
}

void SetMap(const char *restrict name)
{
	if (gHostType && gHostType->setMap)
		gHostType->setMap(name);
}

uint32_t GetNewBattleStatus(void)
{
	int teamBitField=0, ally=0;
	FOR_EACH_PLAYER(s, gMyBattle) {
		ally += ((s->battleStatus & ALLY_MASK) == 0) - ((s->battleStatus & ALLY_MASK) == TO_ALLY_MASK(1));
		teamBitField |= 1 << FROM_TEAM_MASK(s->battleStatus);
	}
	int teamMask = 0;
	for (int i=0; i<16; ++i) {
		if (!(teamBitField & 1 << i)) {
			teamMask = TO_TEAM_MASK(i);
			break;
		}
	}
	return MODE_MASK | READY_MASK | teamMask | TO_ALLY_MASK(ally > 0);
}

void JoinedBattle(Battle *restrict b, uint32_t modHash)
{
	gMyBattle = b;
	gBattleOptions.modHash = modHash;

	gMyUser.battleStatus = 0;
	gLastBattleStatus = 0;

	if (gModHash !=modHash)
		ChangedMod(b->modName);
	if (gMapHash != b->mapHash || !gMapHash)
		ChangedMap(b->mapName);

	BattleRoom_Show();
}

void LeftBattle(void)
{
	BattleRoom_Hide();

	memset(&gBattleOptions, 0, sizeof(gBattleOptions));
	for (int i=0; i<LENGTH(gBattleOptions.positions); ++i)
		gBattleOptions.positions[i] = INVALID_STARTPOS;

	free(currentScript);
	currentScript = NULL;

	BattleRoom_OnSetModDetails();

	while (gMyBattle->nbBots)
		DelBot(gMyBattle->users[gMyBattle->nbParticipants - 1]->bot.name);

	gMyUser.battleStatus = 0;
	gLastBattleStatus = 0;
	battleInfoFinished = 0;

	gMyBattle = NULL;
	if (battleToJoin)
		JoinBattle(battleToJoin, NULL);

	/* TODO: */
	/* *relayHoster = '\0'; */
}


void UpdateBattleStatus(UserOrBot *restrict s, uint32_t bs, uint32_t color)
{
	uint32_t lastBS = s->battleStatus;
	s->battleStatus = bs;
	uint32_t lastColor = s->color;
	s->color = color;

	#ifdef NDEBUG
	if (!s || !gMyBattle || gMyBattle != s->battle)
		return;
	#endif

	BattleRoom_UpdateUser(s);

	if (&s->user == &gMyUser)
		gLastBattleStatus = bs;

	if ((lastBS ^ bs) & MODE_MASK) {
		if (!(bs & MODE_MASK) && BattleRoom_IsAutoUnspec())
			SetBattleStatus(&gMyUser, MODE_MASK, MODE_MASK);
		gMyBattle->nbSpectators = 0;
		for (int i=0; i < gMyBattle->nbParticipants; ++i)
			gMyBattle->nbSpectators += !(gMyBattle->users[i]->battleStatus & MODE_MASK);
		BattleList_UpdateBattle(gMyBattle);
		/* Rebalance(); */
	} else if (bs & MODE_MASK
			&& ((lastBS ^ bs) & (TEAM_MASK | ALLY_MASK)
			   ||  lastColor != color)) {
		/* FixPlayerStatus((void *)s); */
	} else
		return;
	if ((lastBS ^ bs) & MODE_MASK
			|| ((lastBS ^ bs) & ALLY_MASK && &s->user == &gMyUser))
		BattleRoom_StartPositionsChanged();
}

void ChangeOption(Option *restrict opt)
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
		void func(int *i) {
			*i = TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x, point.y, gMainWindow, NULL);
		}
		int clicked;
		SendMessage(gMainWindow, WM_EXECFUNCPARAM, (WPARAM)func, (LPARAM)&clicked);
		if (!clicked)
			return;
		strcpy(val, opt->listItems[clicked - 1].key);
		DestroyMenu(menu);
		break;
	default:
		return;
	}

	if (gHostType && gHostType->setOption)
		gHostType->setOption(opt, opt->val);
}

static void setOptionFromTag(char *restrict key, const char *restrict val)
{
	_strlwr(key);
	if (strcmp(strsep(&key, "/"), "game")) {
		printf("unrecognized script key ?/%s=%s\n", key, val);
		return;
	}

	const char *section = strsep(&key, "/");

	if (!strcmp(section, "startpostype")) {
		StartPosType startPosType = atoi(val);
		if (startPosType != gBattleOptions.startPosType) {
			;// taskSetMinimap = 1;
		}
		gBattleOptions.startPosType = startPosType;
		// PostMessage(gBattleRoom, WM_MOVESTARTPOSITIONS, 0, 0);
		return;
	}

#if 0
	if (!strcmp(s, "team")) {
		int team = atoi(s + sizeof("team") - 1);
		char type = s[sizeof("team/startpos") + (team > 10)];
		((int *)&gBattleOptions.positions[team])[type != 'x'] = atoi(val);
		// PostMessage(gBattleRoom, WM_MOVESTARTPOSITIONS, 0, 0);
		return;
	}
#endif

	if (!strcmp(section, "hosttype")) {
		if (!_stricmp(val, "spads")) {
			gHostType = &gHostSpads;
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
		FOR_EACH_HUMAN_PLAYER(p, gMyBattle) {
			if (!_stricmp(username, p->name)) {
				free(p->skill);
				p->skill = _strdup(val);
			}
		}
		return;
	}

	if (!strcmp(section, "modoptions")) {
		for (int i=0; i<gNbModOptions; ++i) {
			if (strcmp(gModOptions[i].key, key))
				continue;
			free(gModOptions[i].val);
			gModOptions[i].val = _strdup(val);
			BattleRoom_OnSetOption(&gModOptions[i]);
			return;
		}
		printf("unrecognized mod option %s=%s\n", key, val);
		return;
	}

	if (!strcmp(section, "mapoptions")) {
		for (int i=0; i<gNbMapOptions; ++i) {
			if (strcmp(gMapOptions[i].key, key))
				continue;
			free(gMapOptions[i].val);
			gMapOptions[i].val = _strdup(val);
			BattleRoom_OnSetOption(&gMapOptions[i]);
			return;
		}
		printf("unrecognized map option %s=%s\n", key, val);
		return;
	}

	printf("unrecognized script game/%s/%s=%s\n", section, key, val);
}

static void setOptionsFromScript(void)
{
	char *script = _strdup(currentScript);
	char *toFree = script;

	char *key, *val;
	while ((key = strsep(&script, "=")) && (val = strsep(&script, "\t")))
		setOptionFromTag(key, val);

	free(toFree);
}

void AppendScriptTags(char *restrict s)
{
	if (!currentScript) {
		currentScript = _strdup(s);

	} else {
		size_t currentLen = strlen(currentScript) + 1;
		size_t extraLen = strlen(s) + 1;
		currentScript = realloc(currentScript,
				currentLen + extraLen);
		currentScript[currentLen - 1] = '\t';
		memcpy(currentScript + currentLen, s, extraLen);
	}

	if (gModHash)
		setOptionsFromScript();
}

void UpdateModOptions(void)
{
	BattleRoom_OnSetModDetails();

	if (!currentScript)
		return;

	setOptionsFromScript();

	/* Set default values */
	for (int i=0; i<gNbModOptions; ++i)
		if (!gModOptions[i].val)
			BattleRoom_OnSetOption(&gModOptions[i]);

	for (int i=0; i<gNbMapOptions; ++i)
		if (!gMapOptions[i].val)
			BattleRoom_OnSetOption(&gMapOptions[i]);
}
