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

struct Battle;
struct BattleStatus;
struct User;
struct UserBot;
enum ChatType;

typedef enum SplitType {
	SPLIT_HORZ,
	SPLIT_VERT,
	SPLIT_CORNERS1,
	SPLIT_CORNERS2,
	SPLIT_RAND,

	SPLIT_LAST = SPLIT_RAND,
} SplitType;

typedef enum OptionType {
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

	int list_item_len;
	OptionListItem *list_items;
} Option;

#define START_RECT_MAX 200

typedef enum StartPosType {
	STARTPOS_FIXED = 0,
	STARTPOS_RANDOM = 1,
	STARTPOS_CHOOSE_INGAME = 2,
	STARTPOS_CHOOSE_BEFORE = 3,
} StartPosType;

#define INVALID_STARTPOS (StartPos){-1, -1}
#define GET_STARTPOS(_team) (g_battle_options.start_pos_type == STARTPOS_CHOOSE_BEFORE && *(uint64_t *)&g_battle_options.positions[_team] != -1\
		? g_battle_options.positions[_team]\
		: g_map_info.positions[_team])

typedef struct StartRect {
	uint16_t left, top, right, bottom;
} StartRect;

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

void MyBattle_append_script        (char *script);
void MyBattle_change_option        (struct Option *);
struct BattleStatus MyBattle_new_battle_status(void) __attribute__((pure));
void MyBattle_joined_battle        (struct Battle *, uint32_t mod_hash);
void MyBattle_left_battle          (void);
void MyBattle_rebalance            (void);
void MyBattle_reset                (void);
void MyBattle_update_battle_status (struct UserBot *, struct BattleStatus, uint32_t color);
void MyBattle_update_mod_options   (void);

extern void (*MyBattle_force_ally) (const struct User *, int ally_id);
extern void (*MyBattle_force_team) (const struct User *, int team_id);
extern void (*MyBattle_kick)       (const struct UserBot *);
extern void (*MyBattle_said_battle)(struct User *, char *text, enum ChatType);
extern void (*MyBattle_set_map)    (const char *map_name);
extern void (*MyBattle_set_option) (Option *opt, const char *val);
extern void (*MyBattle_set_split)  (int size, SplitType);
extern void (*MyBattle_start_game) (void);

extern MapInfo_       g_map_info;

extern struct Battle *g_my_battle;

extern uint32_t       g_battle_to_join;

extern uint32_t       g_map_hash;
extern uint32_t       g_mod_hash;

extern int            g_map_option_len;
extern Option        *g_map_options;

extern Option        *g_mod_options;
extern int            g_mod_option_len;

extern char         **g_maps;
extern int            g_map_len;

extern char         **g_mods;
extern int            g_mod_len;

extern char           g_side_names[16][32];
extern uint8_t        g_side_len;

extern BattleOption   g_battle_options;
extern uint32_t       g_udp_help_port;
extern bool           g_battle_info_finished;
extern uint32_t       g_last_auto_unspec;

#endif /* end of include guard: BATTLETOOLS_H */
