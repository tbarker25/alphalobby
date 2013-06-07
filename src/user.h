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

#ifndef USER_H
#define USER_H

typedef struct HWND__* HWND;
extern char battleInfoFinished;

enum modeType {
	MODE_SPECTATOR = 0,
	MODE_PLAYER = 1,
};

#define READY_OFFSET 1
#define TEAM_OFFSET 2
#define ALLY_OFFSET 6
#define MODE_OFFSET 10
#define HANDICAP_OFFSET 11
#define SYNC_OFFSET 22
#define SIDE_OFFSET 24
#define AI_OFFSET 31
enum {
	READY_MASK    = 0x01 << READY_OFFSET,
	TEAM_MASK     = 0x0F << TEAM_OFFSET,
	ALLY_MASK     = 0x0F << ALLY_OFFSET,
	MODE_MASK     = 0x01 << MODE_OFFSET,
	HANDICAP_MASK = 0x8F << HANDICAP_OFFSET,
	SYNC_MASK     = 0x03 << SYNC_OFFSET,
	SIDE_MASK     = 0x0F << SIDE_OFFSET,

	AI_MASK       = 0x01 << AI_OFFSET,
};

#define INTERNAL_MASK (AI_MASK)

#define TO_TEAM_MASK(x) ((x) << TEAM_OFFSET)
#define TO_ALLY_MASK(x) ((x) << ALLY_OFFSET)
#define TO_HANDICAP_MASK(x) ((x) << HANDICAP_OFFSET)
#define TO_SIDE_MASK(x) ((x) << SIDE_OFFSET)

#define SYNCED (1 << SYNC_OFFSET)
#define UNSYNCED (2 << SYNC_OFFSET)
#define TO_SIDE_MASK(x) ((x) << SIDE_OFFSET)

#define FROM_TEAM_MASK(x) (((x) & TEAM_MASK) >> TEAM_OFFSET)
#define FROM_ALLY_MASK(x) (((x) & ALLY_MASK) >> ALLY_OFFSET)
#define FROM_HANDICAP_MASK(x) (((x) & HANDICAP_MASK) >> HANDICAP_OFFSET)
#define FROM_SIDE_MASK(x) (((x) & SIDE_MASK) >> SIDE_OFFSET)

#define INGAME_OFFSET 0
#define AWAY_OFFSET 1
#define RANK_OFFSET 2
#define ACCESS_OFFSET 5
#define BOT_OFFSET 6

enum {
	CS_INGAME_MASK = 0x01 << INGAME_OFFSET,
	CS_AWAY_MASK  = 0x01 << AWAY_OFFSET,
	CS_RANK_MASK  = 0x07 << RANK_OFFSET,
	CS_ACCESS_MASK  = 0x01 << ACCESS_OFFSET,
	CS_BOT_MASK  = 0x01 << BOT_OFFSET,
};

#define TO_INGAME_MASK(x) ((x) << INGAME_OFFSET)
#define TO_AWAY_MASK(x) ((x) << AWAY_OFFSET)
#define TO_RANK_MASK(x) ((x) << RANK_OFFSET)
#define TO_ACCESS_MASK(x) ((x) << ACCESS_OFFSET)
#define TO_BOT_MASK(x) ((x) << BOT_OFFSET)

#define FROM_RANK_MASK(x) (((x) & CS_RANK_MASK) >> RANK_OFFSET)

enum USER_SYNC_STATUS {
	USER_SYNC_UNKNOWN = 0,
	USER_SYNC_SYNCED = 1,
	USER_SYNC_UNSYNCED = 2,
};

#define UNTAGGED_NAME(name) ((name)[0] == '[' ? strchr((name), ']') + 1 : (name))

typedef struct UserBot {
	char name[MAX_NAME_LENGTH_NUL];
	uint32_t battleStatus;
	struct Battle *battle;
	union {
		uint32_t color;
		struct {
			uint8_t red, green, blue;
		};
	};
} UserBot;

typedef struct User {
	UserBot;

	HWND chatWindow;
	uint32_t cpu, id;

	char alias[MAX_NAME_LENGTH_NUL];
	char *scriptPassword;
	uint8_t clientStatus, country, ignore;
}User;

typedef struct Bot {
	UserBot;

	char *dll;
	int nbOptions;
	struct Option2 *options;
	User *owner;
	struct Bot *next;
} Bot;

//Identify them with AI_MASK of battleStatus
typedef union UserOrBot {
	UserBot;
	User user;
	Bot bot;
}UserOrBot;

extern User gMyUser;

User * FindUser(const char username[]);
User * NewUser(uint32_t id, const char *name);
User *GetNextUser(void);
void DelUser(User *u);
void AddBot(const char *name, User *owner, uint32_t battleStatus, uint32_t color, const char *aiDll);
void DelBot(const char *name);
void UpdateBattleStatus(UserOrBot *s, uint32_t battleStatus, uint32_t color);
void ResetUsers(void);

#endif /* end of include guard: USER_H */
