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
	((uint8_t)((_b)->user_len - (_b)->spectator_len))

typedef enum BattleType {
	BATTLE_NORMAL = 0,
	BATTLE_REPLAY = 1,
} BattleType;

typedef enum NatType {
	NAT_NONE  = 0,
	NAT_OTHER,
} NatType;

typedef struct Battle {
	uint32_t         id;
	uint32_t         map_hash;
	char            *map_name;
	char            *title;
	struct User     *founder;
	struct Bot      *first_bot;
	uint16_t         port;
	char             ip[16];
	uint8_t          max_players;
	uint8_t          rank;
	uint8_t          user_len;
	uint8_t          spectator_len;
	bool             passworded;
	bool             locked;
	enum BattleType  type;
	enum NatType     nat_type;
	char             mod_name[];
} Battle;

void     Battles_del(Battle *);
Battle * Battles_find(uint32_t id) __attribute__((pure));
Battle * Battles_new(uint32_t id, const char *title, const char *mod_name);
void     Battles_reset(void);

#define Battles_for_each_human(__u, __b) \
	for (User *__u = __b->founder; __u != (User *)__b->first_bot; __u = __u->next_in_battle)

#define Battles_for_each_user(__u, __b) \
	for (UserBot *__u = __b->founder; __u; __u = __u->next_in_battle)

#endif /* end of include guard: BATTLE_H */
