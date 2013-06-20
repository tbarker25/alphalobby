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

#define UNTAGGED_NAME(name) ((name)[0] == '[' ? strchr((name), ']') + 1 : (name))

typedef struct HWND__* HWND;

typedef struct __attribute__((packed)) BattleStatus {
	unsigned int          : 1;
	unsigned int ready    : 1;
	unsigned int team     : 4;
	unsigned int ally     : 4;
	unsigned int mode     : 1;
	unsigned int handicap : 7;
	unsigned int          : 4;
	unsigned int sync     : 2;
	unsigned int side     : 4;
	unsigned int          : 4;
} BattleStatus;

typedef struct __attribute__((packed, aligned(1))) ClientStatus {
	unsigned int ingame : 1;
	unsigned int away   : 1;
	unsigned int rank   : 3;
	unsigned int access : 1;
	unsigned int bot    : 1;
	unsigned int        : 1;
} ClientStatus;

typedef enum SyncStatus {
	SYNC_UNKNOWN = 0,
	SYNC_SYNCED = 1,
	SYNC_UNSYNCED = 2,
} SyncStatus;

typedef struct UserBot {
	char name[MAX_NAME_LENGTH_NUL];
	bool ai;
	BattleStatus;
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
	char *script_password;
	char *skill;
	char alias[MAX_NAME_LENGTH_NUL];
	bool ignore;
	uint8_t country;
	ClientStatus;
} User;

typedef struct Bot {
	UserBot;

	char *dll;
	int option_len;
	struct Option2 *options;
	User *owner;
	struct Bot *next;
} Bot;

void   Users_add_bot(const char *name, User *owner, BattleStatus battle_status, uint32_t color, const char *aiDll);
void   Users_del(User *);
void   Users_del_bot(const char *name);
User * Users_find(const char username[]) __attribute__((pure));
User * Users_get_next(void);
User * Users_new(uint32_t id, const char *name);
void   Users_reset(void);

extern User g_my_user;

_Static_assert(sizeof(BattleStatus) == 4, "BattleStatus should be 4 bytes");
_Static_assert(sizeof(ClientStatus) == 1, "ClientStatus should be 1 byte");

#endif /* end of include guard: USER_H */
