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
extern bool g_battle_info_finished;

enum ModeType {
	MODE_SPECTATOR = 0,
	MODE_PLAYER = 1,
};

enum {
	BS_READY    = 0x01 << 1,
	BS_TEAM     = 0x0F << 2,
	BS_ALLY     = 0x0F << 6,
	BS_MODE     = 0x01 << 10,
	BS_HANDICAP = 0x8F << 11,
	BS_SYNC     = 0x03 << 22,
	BS_SIDE     = 0x0F << 24,

	BS_AI       = 0x01 << 31,
};

#define BS_INTERNAL (BS_AI)

#define TO_BS_TEAM_MASK(x)     ((x) << 2)
#define TO_BS_ALLY_MASK(x)     ((x) << 6)
#define TO_BS_HANDICAP_MASK(x) ((x) << 11)
#define TO_BS_SIDE_MASK(x)     ((x) << 24)

#define SYNCED          (1 << 22)
#define UNSYNCED        (2 << 22)
#define TO_BS_SIDE_MASK(x) ((x) << 24)

#define FROM_BS_TEAM(x)     (((x) & BS_TEAM)     >> 2)
#define FROM_BS_ALLY(x)     (((x) & BS_ALLY)     >> 6)
#define FROM_BS_HANDICAP(x) (((x) & BS_HANDICAP) >> 11)
#define FROM_BS_SIDE(x)     (((x) & BS_SIDE)     >> 24)

enum {
	CS_INGAME = 0x01 << 0,
	CS_AWAY   = 0x01 << 1,
	CS_RANK   = 0x07 << 2,
	CS_ACCESS = 0x01 << 5,
	CS_BOT    = 0x01 << 6,
};

#define TO_INGAME_MASK(x) ((x) << 0)
#define TO_AWAY_MASK(x)   ((x) << 1)
#define TO_RANK_MASK(x)   ((x) << 2)
#define TO_ACCESS_MASK(x) ((x) << 5)
#define TO_BOT_MASK(x)    ((x) << 6)

#define FROM_RANK_MASK(x) (((x) & CS_RANK) >> 2)

enum USER_SYNC_STATUS {
	USER_SYNC_UNKNOWN = 0,
	USER_SYNC_SYNCED = 1,
	USER_SYNC_UNSYNCED = 2,
};

#define UNTAGGED_NAME(name) ((name)[0] == '[' ? strchr((name), ']') + 1 : (name))

typedef struct UserBot {
	char name[MAX_NAME_LENGTH_NUL];
	uint32_t battle_status;
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

	HWND chat_window;
	uint32_t cpu, id;

	char alias[MAX_NAME_LENGTH_NUL];
	char *script_password;
	uint8_t client_status, country, ignore;
	char *skill;
}User;

typedef struct Bot {
	UserBot;

	char *dll;
	int option_len;
	struct Option2 *options;
	User *owner;
	struct Bot *next;
} Bot;

//Identify them with BS_AI of battle_status
typedef union UserOrBot {
	UserBot;
	User user;
	Bot bot;
}UserOrBot;

extern User g_my_user;

void   Users_add_bot(const char *name, User *owner, uint32_t battle_status, uint32_t color, const char *aiDll);
void   Users_del(User *u);
void   Users_del_bot(const char *name);
User * Users_find(const char username[]) __attribute__((pure));
User * Users_get_next(void);
User * Users_new(uint32_t id, const char *name);
void   Users_reset(void);

#endif /* end of include guard: USER_H */
