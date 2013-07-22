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

#ifndef BATTLE_H
#define BATTLE_H

/* Also used for mapnames, modnames. Overflow is truncated to fit */
#define MAX_TITLE 128

#define GetNumPlayers(_b)\
	((uint8_t)((_b)->user_len - (_b)->spectator_len - (_b)->bot_len))


enum BattleType {
	BATTLE_NORMAL = 0,
	BATTLE_REPLAY = 1,
};

enum NatType {
	NAT_NONE  = 0,
	NAT_OTHER,
};

typedef struct Battle {
	uint32_t id;
	uint32_t map_hash;
	char map_name[MAX_TITLE+1], mod_name[MAX_TITLE+1], title[MAX_TITLE+1], ip[16];
	uint16_t port;
	uint8_t passworded, locked, type, max_players, rank, user_len, spectator_len, bot_len;
	enum NatType nat_type;
	union {
		struct UserBot *users[128];
		struct User *founder;
	};
} Battle;

void     Battles_del(Battle *);
Battle * Battles_find(uint32_t id) __attribute__((pure));
Battle * Battles_new(uint32_t id);
void     Battles_reset(void);

#endif /* end of include guard: BATTLE_H */
