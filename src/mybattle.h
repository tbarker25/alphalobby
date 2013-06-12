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

	size_t list_item_len;
	OptionListItem *list_items;
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
#define GET_STARTPOS(_team) (g_battle_options.start_pos_type == STARTPOS_CHOOSE_BEFORE && *(uint64_t *)&g_battle_options.positions[_team] != -1\
		? g_battle_options.positions[_team]\
		: g_map_info.positions[_team])

typedef struct StartRect {
	uint16_t left, top, right, bottom;
} StartRect;

typedef struct HostType {
	void (*force_ally)(const char *name, int ally_id);
	void (*force_team)(const char *name, int team_id);
	void (*kick)(const char *name);
	void (*said_battle)(const char *username, char *text);
	void (*set_map)(const char *map_name);
	void (*set_option)(Option *opt, const char *val);
	void (*set_split)(int size, SplitType type);
	void (*start_game)(void);
} HostType;

typedef struct BattleOptions {
	StartPosType start_pos_type;
	StartRect start_rects[NUM_ALLIANCES];
	StartPos positions[NUM_ALLIANCES];
	uint32_t mod_hash;
} BattleOption;


typedef struct MapInfo_ {
	MapInfo;
	char _description[256];
	char _author[201];
} MapInfo_;

union UserOrBot;
struct Battle;
struct BattleStatus;

extern MapInfo_ g_map_info;

struct Battle;
extern struct Battle *g_my_battle;
extern uint32_t g_battle_to_join;

extern size_t g_mod_option_len, g_map_option_len;
extern Option *g_mod_options, *g_map_options;
extern uint32_t g_map_hash, g_mod_hash;

extern char **g_maps, **g_mods;
extern size_t g_map_len, g_mod_len;

extern BattleOption g_battle_options;
extern const HostType *g_host_type;

extern uint8_t g_side_len;
extern char g_side_names[16][32];

extern uint32_t g_udp_help_port;

extern bool g_battle_info_finished;

void MyBattle_append_script(char *restrict script);
void MyBattle_change_option(struct Option *restrict opt);
struct BattleStatus MyBattle_new_battle_status(void)
	__attribute__((pure));
void MyBattle_joined_battle(struct Battle *restrict b, uint32_t mod_hash);
void MyBattle_leave(void);
void MyBattle_left_battle(void);
void Rebalance(void);
void ResetBattleOptions(void);
void MyBattle_set_map(const char *name);
void MyBattle_set_split(SplitType, int size);
void MyBattle_update_battle_status(union UserOrBot *s, struct BattleStatus battle_status, uint32_t color);
void MyBattle_update_mod_options(void);


#endif /* end of include guard: BATTLETOOLS_H */
