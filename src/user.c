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

#include <assert.h>
#include <inttypes.h>
#include <malloc.h>
#include <search.h>
#include <stdio.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "common.h"
#include "minimap.h"
#include "mybattle.h"
#include "settings.h"
#include "user.h"

#define ALLOC_STEP 128

/* change this if alias.conf format changes */
#define MAGIC_NUMBER 0xfd0df209

typedef struct __attribute__((packed)) AliasEntry {
	uint32_t id;
	char alias[MAX_NAME_LENGTH_NUL];
} AliasEntry;

typedef struct AliasFile {
	uint32_t magic_number;
	uint32_t len;
	AliasEntry aliases[];
} AliasFile;

static char * get_alias   (uint32_t id);
static int    compare_int (const void *, const void *);

User                g_my_user;

static User       **s_users;
static size_t       s_users_len;

static AliasEntry  *s_aliases;
static uint32_t     s_aliases_len;
static HANDLE       s_alias_file_handle;
static HANDLE       s_alias_view;
static size_t       s_extra_len;

User *
Users_find(const char *name)
{
	if (!strcmp(name, g_my_user.name))
		return &g_my_user;
	for (size_t i=0; i<s_users_len; ++i)
		if (!strcmp(s_users[i]->name, name))
			return &(*s_users[i]);
	return NULL;
}

User *
Users_get_next(void)
{
	static size_t last;
	return last < s_users_len ? s_users[last++] : (last = 0, NULL);
}

User *
Users_new(uint32_t id, const char *name)
{
	User *u;

	if (!strcmp(g_my_user.name, name)) {
		g_my_user.id = id;
		return &g_my_user;
	}

	for (size_t i=0; i<s_users_len; ++i)
		if (s_users[i]->id == id)
			return s_users[i];

	if (s_users_len % ALLOC_STEP == 0)
		s_users = realloc(s_users, (s_users_len + ALLOC_STEP) * sizeof *s_users);
	u = s_users[s_users_len++] = calloc(1, sizeof **s_users);
	u->id = id;
	u->alias = get_alias(id);
	if (!*u->alias)
		strncpy(u->alias, UNTAGGED_NAME(name), MAX_NAME_LENGTH_NUL);
	return u;
}

void
Users_del(User *u)
{
	u->name[0] = 0;
}

void
Users_add_bot(const char *name, User *owner, BattleStatus battle_status,
		uint32_t color, const char *dll)
{
	Bot *bot;
	Battle *b;
	int i;

	bot               = calloc(1, sizeof *bot);
	strcpy(bot->name, name);
	bot->owner        = owner;
	bot->BattleStatus = battle_status;
	bot->ai           = 1;
	bot->mode         = 1;
	bot->color        = color;
	bot->dll          = _strdup(dll);

	b = g_my_battle;
	i = b->user_len - b->bot_len;
	while (i<b->user_len
	    && _stricmp(b->users[i]->name, bot->name) < 0)
		++i;
	for (int j=b->user_len; j>i; --j)
		b->users[j] = b->users[j-1];
	b->users[i] = (UserBot *)bot;
	++b->user_len;
	++b->bot_len;

	Minimap_on_start_position_change();
	BattleRoom_update_user((void *)bot);
}

void
Users_del_bot(const char *name)
{
	Bot *b;
	int i;

	i = g_my_battle->user_len - g_my_battle->bot_len;
	while (i < g_my_battle->user_len
			&& _stricmp(g_my_battle->users[i]->name, name) < 0)
		++i;
	BattleRoom_on_left_battle(g_my_battle->users[i]);
	b = (Bot *)g_my_battle->users[i];
	free(b->dll);
	free(b->options);
	free(b);
	while (++i < g_my_battle->user_len)
		g_my_battle->users[i - 1] = g_my_battle->users[i];
	--g_my_battle->user_len;
	--g_my_battle->bot_len;
	Minimap_on_start_position_change();
}

void
Users_reset(void)
{
	for (size_t i=0; i<s_users_len; ++i)
		*s_users[i]->name = '\0';
}

void
Users_cleanup(void)
{
	AliasFile *file;
	size_t len = s_aliases_len + s_extra_len;

	file = (AliasFile *)((char *)s_aliases - offsetof(AliasFile, aliases));
	qsort(s_aliases, len, sizeof *s_aliases, compare_int);
	file->len = len;
	file->magic_number = MAGIC_NUMBER;
	UnmapViewOfFile(file);
	CloseHandle(s_alias_view);
	CloseHandle(s_alias_file_handle);
}

static char *
get_alias(uint32_t id)
{
	AliasEntry *result;

	if (!s_aliases) {
		AliasFile *file;

		s_alias_file_handle = CreateFile(Settings_get_data_dir(L"aliases.conf"),
		    GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
		    FILE_ATTRIBUTE_NORMAL, NULL);
		/* TODO: Real error handling */
		assert(s_alias_file_handle);

		s_alias_view = CreateFileMapping(s_alias_file_handle, NULL,
		    PAGE_READWRITE, 0, 1 << 20, NULL);
		assert(s_alias_view);

		file = MapViewOfFile(s_alias_view, FILE_MAP_WRITE, 0, 0, 0);
		assert(file);

		if (file->magic_number == MAGIC_NUMBER)
			s_aliases_len = file->len;
		s_aliases = file->aliases;
	}

	result = bsearch(&id, s_aliases, s_aliases_len, sizeof *s_aliases, compare_int);
	if (result)
		return result->alias;
	result = &s_aliases[s_aliases_len + s_extra_len++];
	result->id = id;
	*result->alias = '\0';
	return result->alias;
}

static int
compare_int(const void *a, const void *b)
{
	return *(int32_t *)a - *(int32_t *)b;
}
