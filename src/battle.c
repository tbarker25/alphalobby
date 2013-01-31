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
#include <malloc.h>
#include <inttypes.h>
#include <unistd.h>

#include <windows.h>

#include "data.h"

#define ALLOC_STEP 10
#define LENGTH(x) (sizeof(x) / sizeof(*x))

static size_t nbBattles;
static Battle **battles;

Battle * FindBattle(uint32_t id)
{
	for (int i=0; i<nbBattles; ++i)
		if (battles[i] && battles[i]->id == id)
			return battles[i];
	return NULL;
}

Battle *NewBattle(void)
{
	int i=0;
	for (; i<nbBattles; ++i) {
		if (battles[i] == NULL)
			break;
	}
	if (i == nbBattles) {
		if (nbBattles % ALLOC_STEP == 0)
			battles = realloc(battles, (nbBattles + ALLOC_STEP) * sizeof(Battle *));
		++nbBattles;
	}
	battles[i] = calloc(1, sizeof(Battle));
	return battles[i];
}

void DelBattle(Battle *b)
{
	free(b);
	for (int i=0; i<nbBattles; ++i) {
		if (battles[i] == b) {
			battles[i] = NULL;
			return;
		}
	}
	assert(0);
}

void ResetBattles(void)
{
	for (int i=0; i<nbBattles; ++i)
		free(battles[i]);
	nbBattles = 0;
	free(battles);
	battles = NULL;
}
