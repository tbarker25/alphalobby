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

enum {
	READY_MASK    = 0x01 << 1,
	TEAM_MASK     = 0x0F << 2,
	ALLY_MASK     = 0x0F << 6,
	MODE_MASK     = 0x01 << 10,
	HANDICAP_MASK = 0x8F << 11,
	SYNC_MASK     = 0x03 << 22,
	SIDE_MASK     = 0x0F << 24,

	AI_MASK       = 0x01 << 31,
};

#define INTERNAL_MASK (AI_MASK)

#define TO_TEAM_MASK(x)     ((x) << 2)
#define TO_ALLY_MASK(x)     ((x) << 6)
#define TO_HANDICAP_MASK(x) ((x) << 11)
#define TO_SIDE_MASK(x)     ((x) << 24)

#define SYNCED          (1 << 22)
#define UNSYNCED        (2 << 22)
#define TO_SIDE_MASK(x) ((x) << 24)

#define FROM_TEAM_MASK(x)     (((x) & TEAM_MASK)     >> 2)
#define FROM_ALLY_MASK(x)     (((x) & ALLY_MASK)     >> 6)
#define FROM_HANDICAP_MASK(x) (((x) & HANDICAP_MASK) >> 11)
#define FROM_SIDE_MASK(x)     (((x) & SIDE_MASK)     >> 24)

enum {
	CS_INGAME_MASK = 0x01 << 0,
	CS_AWAY_MASK   = 0x01 << 1,
	CS_RANK_MASK   = 0x07 << 2,
	CS_ACCESS_MASK = 0x01 << 5,
	CS_BOT_MASK    = 0x01 << 6,
};

#define TO_INGAME_MASK(x) ((x) << 0)
#define TO_AWAY_MASK(x)   ((x) << 1)
#define TO_RANK_MASK(x)   ((x) << 2)
#define TO_ACCESS_MASK(x) ((x) << 5)
#define TO_BOT_MASK(x)    ((x) << 6)

#define FROM_RANK_MASK(x) (((x) & CS_RANK_MASK) >> 2)

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
	char *skill;
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

User * FindUser(const char username[])
	__attribute__((pure));
User * NewUser(uint32_t id, const char *name);
User * GetNextUser(void);
void DelUser(User *u);
void AddBot(const char *name, User *owner, uint32_t battleStatus, uint32_t color, const char *aiDll);
void DelBot(const char *name);
void UpdateBattleStatus(UserOrBot *s, uint32_t battleStatus, uint32_t color);
void ResetUsers(void);

#endif /* end of include guard: USER_H */
