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

#define MAX_TITLE 128 //Also used for mapnames, modnames. Overflow is truncated to fit

enum BattleType {
	BATTLE_NORMAL = 0,
	BATTLE_REPLAY = 1,
};

enum NatType {
	NAT_NONE  = 0,
	NAT_OTHER,
};

typedef struct Battle {
	//MUST BE FIRST SINCE IT IS USED BY HASH:
	uint32_t id;
	uint32_t map_hash;
	char map_name[MAX_TITLE+1], mod_name[MAX_TITLE+1], title[MAX_TITLE+1], ip[16];
	uint16_t port;
	uint8_t passworded, locked, type, max_players, rank, nbParticipants, nbSpectators, nbBots;
	enum NatType nat_type;
	union {
		union UserOrBot *users[128];
		struct User *founder;
	};
} Battle;

void     Battles_del(Battle *);
Battle * Battles_find(uint32_t id) __attribute__((pure));
Battle * Battles_new(void);
void     Battles_reset(void);

#define FOR_EACH_PARTICIPANT(_u, _b)\
	for (UserOrBot **__u = (_b)->users, *(_u); (_u) = *__u, __u - (_b)->users < (_b)->nbParticipants; ++__u)

#define FOR_EACH_USER(_u, _b)\
	for (User **__u = (User **)(_b)->users, *(_u); (_u) = *__u, __u - (User **)(_b)->users < (_b)->nbParticipants - (_b)->nbBots; ++__u)

#define FOR_EACH_BOT(_u, _b)\
	for (User **__u = (Bot **)&(_b)->users[(_b)->nbBots], *(_u); (_u) = *__u, __u - (Bot **)(_b)->users < (_b)->nbParticipants; ++__u)

#define FOR_EACH_PLAYER(_u, _b)\
	for (UserOrBot **__u = (_b)->users, *(_u); (_u) = *__u, __u - (_b)->users < (_b)->nbParticipants; ++__u) if (!((_u)->battle_status & BS_MODE)) continue; else

#define FOR_EACH_HUMAN_PLAYER(_u, _b)\
	for (User **__u = (User **)(_b)->users, *(_u); (_u) = *__u, __u - (User **)(_b)->users < (_b)->nbParticipants - (_b)->nbBots; ++__u) if (!((_u)->battle_status & BS_MODE)) continue; else


#define GetNumPlayers(_b)\
	((_b)->nbParticipants - (_b)->nbSpectators - (_b)->nbBots)

#endif /* end of include guard: BATTLE_H */
