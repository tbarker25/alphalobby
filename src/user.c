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

#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "common.h"
#include "mybattle.h"
#include "user.h"

#define ALLOC_STEP 10

User g_my_user;
static User **users;
static size_t user_count;
bool g_battle_info_finished;

User * Users_find(const char *name)
{
	if (!strcmp(name, g_my_user.name))
		return &g_my_user;
	for (int i=0; i<user_count; ++i)
		if (!strcmp(users[i]->name, name))
			return &(*users[i]);
	return NULL;
}

User *Users_get_next(void)
{
	static int
last;
	return last < user_count ? users[last++] : (last = 0, NULL);
}

User * Users_new(uint32_t id, const char *name)
{
	if (!strcmp(g_my_user.name, name)) {
		g_my_user.id = id;
		return &g_my_user;
	}
	int i=0;
	for (; i<user_count; ++i)
		if (users[i]->id == id)
			break;

	if (i == user_count) {
		if (user_count % ALLOC_STEP == 0)
			users = realloc(users, (user_count + ALLOC_STEP) * sizeof(User *));
		++user_count;
		users[i] = calloc(1, sizeof(User));
		users[i]->id = id;
		strcpy(users[i]->alias, UNTAGGED_NAME(name));
	}
	SetWindowTextA(users[i]->chat_window, name);
	return users[i];
}

void
Users_del(User *u)
{
	u->name[0] = 0;
}

void
Users_add_bot(const char *name, User *owner, uint32_t battle_status,
		uint32_t color, const char *aiDll)
{
	Bot *bot = calloc(1, sizeof(*bot));
	strcpy(bot->name, name);
	bot->owner = owner;
	bot->battle_status = battle_status | BS_AI | BS_MODE;
	bot->color = color;
	bot->dll = _strdup(aiDll);

	Battle *b = g_my_battle;
	int i=b->participant_count - b->bot_count;
	while (i<b->participant_count
			&& _stricmp(b->users[i]->name, bot->name) < 0)
		++i;
	for (int j=b->participant_count; j>i; --j)
		b->users[j] = b->users[j-1];
	b->users[i] = (UserOrBot *)bot;
	++b->participant_count;
	++b->bot_count;

	/* Rebalance(); */
	BattleRoom_on_start_position_change();
	BattleRoom_update_user((void *)bot);
}

void
Users_del_bot(const char *name)
{
	int i = g_my_battle->participant_count - g_my_battle->bot_count;
	while (i < g_my_battle->participant_count
			&& _stricmp(g_my_battle->users[i]->name, name) < 0)
		++i;
	BattleRoom_on_left_battle(g_my_battle->users[i]);
	free(g_my_battle->users[i]->bot.dll);
	free(g_my_battle->users[i]->bot.options);
	free(g_my_battle->users[i]);
	while (++i < g_my_battle->participant_count)
		g_my_battle->users[i - 1] = g_my_battle->users[i];
	--g_my_battle->participant_count;
	--g_my_battle->bot_count;
	/* Rebalance(); */
	BattleRoom_on_start_position_change();
}

void
Users_reset(void)
{
	for (int i=0; i<user_count; ++i)
		*users[i]->name = '\0';
}
