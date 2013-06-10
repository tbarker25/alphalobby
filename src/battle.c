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
#include <stdbool.h>
#include <unistd.h>

#include <windows.h>

#include "common.h"
#include "user.h"
#include "battle.h"
#include "mybattle.h"

#define ALLOC_STEP 10
#define LENGTH(x) (sizeof(x) / sizeof(*x))

static size_t battle_len;
static Battle **battles;

Battle * Battles_find(uint32_t id)
{
	for (size_t i=0; i<battle_len; ++i)
		if (battles[i] && battles[i]->id == id)
			return battles[i];
	return NULL;
}

Battle *Battles_new(void)
{
	size_t i=0;

	for (; i<battle_len; ++i) {
		if (battles[i] == NULL)
			break;
	}
	if (i == battle_len) {
		if (battle_len % ALLOC_STEP == 0)
			battles = realloc(battles, (battle_len + ALLOC_STEP) * sizeof(Battle *));
		++battle_len;
	}
	battles[i] = calloc(1, sizeof(Battle));
	return battles[i];
}

void
Battles_del(Battle *b)
{
	free(b);
	for (size_t i=0; i<battle_len; ++i) {
		if (battles[i] == b) {
			battles[i] = NULL;
			return;
		}
	}
	assert(0);
}

void
Battles_reset(void)
{
	for (size_t i=0; i<battle_len; ++i)
		free(battles[i]);
	battle_len = 0;
	free(battles);
	battles = NULL;
}
