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

#ifndef BATTLETOOLS_H
#define BATTLETOOLS_H

#include "unitsync.h"

#define NUM_TEAMS 16
#define NUM_ALLIANCES 16

typedef enum SplitType {
	SPLIT_HORZ,
	SPLIT_VERT,
	SPLIT_CORNERS1,
	SPLIT_CORNERS2,
	SPLIT_RAND,

	SPLIT_LAST = SPLIT_RAND,
} SplitType;

typedef enum OptionType{
  opt_error = 0, opt_bool = 1, opt_list = 2, opt_number = 3,
  opt_string = 4, opt_section = 5
} OptionType;

typedef struct OptionListItem {
	char *key, *name;
} OptionListItem;

typedef struct Option {
	OptionType type;
	char *key, *name, *val, *def, *desc;
	struct Option *section;

	size_t nbListItems;
	OptionListItem *listItems;
} Option;

#define START_RECT_MAX 200

typedef enum StartPosType {
	STARTPOS_FIXED = 0,
	STARTPOS_RANDOM = 1,
	STARTPOS_CHOOSE_INGAME = 2,
	STARTPOS_CHOOSE_BEFORE = 3,
} StartPosType;

#define HOST_RELAY 0x01
#define HOST_LOCAL 0x03
// #define HOST_SP    0x05
#define HOST_FLAG  0x01

#define INVALID_STARTPOS (StartPos){-1, -1}
#define GET_STARTPOS(_team) (gBattleOptions.startPosType == STARTPOS_CHOOSE_BEFORE && *(uint64_t *)&gBattleOptions.positions[_team] != -1\
		? gBattleOptions.positions[_team]\
		: gMapInfo.positions[_team])

typedef struct StartRect {
	uint16_t left, top, right, bottom;
} StartRect;

typedef struct HostType {
	void (*forceAlly)(const char *name, int allyId);
	void (*forceTeam)(const char *name, int teamId);
	void (*kick)(const char *name);
	void (*saidBattle)(const char *userName, char *text);
	void (*setMap)(const char *mapName);
	void (*setOption)(Option *opt, const char *val);
	void (*setSplit)(int size, SplitType type);
} HostType;

typedef struct BattleOptions {
	StartPosType startPosType;
	StartRect startRects[NUM_ALLIANCES];
	StartPos positions[NUM_ALLIANCES];
	uint32_t modHash;
} BattleOption;


struct _LargeMapInfo {
	MapInfo mapInfo;
	char description[256];
	char author[201];
};

union UserOrBot;
struct Battle;

extern struct _LargeMapInfo _gLargeMapInfo;
#define gMapInfo (_gLargeMapInfo.mapInfo)

struct Battle;
extern struct Battle *gMyBattle;
extern uint32_t battleToJoin;

extern ssize_t gNbModOptions, gNbMapOptions;
extern Option *gModOptions, *gMapOptions;
extern uint32_t gMapHash, gModHash;

extern char **gMaps, **gMods;
extern ssize_t gNbMaps, gNbMods;

extern BattleOption gBattleOptions;
extern const HostType *gHostType;

extern uint8_t gNbSides;
extern char gSideNames[16][32];

extern uint32_t gUdpHelpPort;

void SetSplit(SplitType, int size);
void SetMap(const char *name);
void FixPlayerStatus(const union UserOrBot *u);
//use NULL to fix all players in battle

void Rebalance(void);
//Calls FixPlayerStatus(NULL)

uint32_t GetNewBattleStatus(void);
void JoinedBattle(struct Battle *b, uint32_t modHash);
void ChangeOption(struct Option *opt);
void ResetBattleOptions(void);
void LeftBattle(void);
void UpdateModOptions(void);
void AppendScriptTags(char *script);


#endif /* end of include guard: BATTLETOOLS_H */
