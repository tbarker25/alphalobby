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

#include <stdio.h>
#include <inttypes.h>
#include <malloc.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "common.h"
#include "mybattle.h"
#include "user.h"

#define ALLOC_STEP 10

User gMyUser;
static User **users;
static size_t nbUsers;
char battleInfoFinished;

User * FindUser(const char *name)
{
	if (!strcmp(name, gMyUser.name))
		return &gMyUser;
	for (int i=0; i<nbUsers; ++i)
		if (!strcmp(users[i]->name, name))
			return &(*users[i]);
	return NULL;
}

User *GetNextUser(void)
{
	static int last;
	return last < nbUsers ? users[last++] : (last = 0, NULL);
}

User * NewUser(uint32_t id, const char *name)
{
	if (!strcmp(gMyUser.name, name)) {
		gMyUser.id = id;
		return &gMyUser;
	}
	int i=0;
	for (; i<nbUsers; ++i)
		if (users[i]->id == id)
			break;

	if (i == nbUsers) {
		if (nbUsers % ALLOC_STEP == 0)
			users = realloc(users, (nbUsers + ALLOC_STEP) * sizeof(User *));
		++nbUsers;
		users[i] = calloc(1, sizeof(User));
		users[i]->id = id;
		strcpy(users[i]->alias, UNTAGGED_NAME(name));
	}
	SetWindowTextA(users[i]->chatWindow, name);
	return users[i];
}

void DelUser(User *u)
{
	u->name[0] = 0;
}

void AddBot(const char *name, User *owner, uint32_t battleStatus,
		uint32_t color, const char *aiDll)
{
	Bot *bot = calloc(1, sizeof(*bot));
	strcpy(bot->name, name);
	bot->owner = owner;
	bot->battleStatus = battleStatus | AI_MASK | MODE_MASK;
	bot->color = color;
	bot->dll = _strdup(aiDll);

	Battle *b = gMyBattle;
	int i=b->nbParticipants - b->nbBots;
	while (i<b->nbParticipants
			&& _stricmp(b->users[i]->name, bot->name) < 0)
		++i;
	for (int j=b->nbParticipants; j>i; --j)
		b->users[j] = b->users[j-1];
	b->users[i] = (UserOrBot *)bot;
	++b->nbParticipants;
	++b->nbBots;

	/* Rebalance(); */
	BattleRoom_StartPositionsChanged();
	BattleRoom_UpdateUser((void *)bot);
}

void DelBot(const char *name)
{
	int i = gMyBattle->nbParticipants - gMyBattle->nbBots;
	while (i < gMyBattle->nbParticipants
			&& _stricmp(gMyBattle->users[i]->name, name) < 0)
		++i;
	BattleRoom_RemoveUser(gMyBattle->users[i]);
	free(gMyBattle->users[i]->bot.dll);
	free(gMyBattle->users[i]->bot.options);
	free(gMyBattle->users[i]);
	while (++i < gMyBattle->nbParticipants)
		gMyBattle->users[i - 1] = gMyBattle->users[i];
	--gMyBattle->nbParticipants;
	--gMyBattle->nbBots;
	/* Rebalance(); */
	BattleRoom_StartPositionsChanged();
}

void ResetUsers(void)
{
	for (int i=0; i<nbUsers; ++i)
		*users[i]->name = '\0';
}
