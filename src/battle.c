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

static size_t g_battles_len;
static Battle **g_battles;

Battle *
Battles_find(uint32_t id)
{
	for (size_t i=0; i<g_battles_len; ++i)
		if (g_battles[i] && g_battles[i]->id == id)
			return g_battles[i];
	return NULL;
}

Battle *
Battles_new(void)
{
	size_t i=0;

	for (; i<g_battles_len; ++i) {
		if (g_battles[i] == NULL)
			break;
	}
	if (i == g_battles_len) {
		if (g_battles_len % ALLOC_STEP == 0)
			g_battles = realloc(g_battles, (g_battles_len + ALLOC_STEP) * sizeof(Battle *));
		++g_battles_len;
	}
	g_battles[i] = calloc(1, sizeof(Battle));
	return g_battles[i];
}

void
Battles_del(Battle *b)
{
	free(b);
	for (size_t i=0; i<g_battles_len; ++i) {
		if (g_battles[i] == b) {
			g_battles[i] = NULL;
			return;
		}
	}
	assert(0);
}

void
Battles_reset(void)
{
	for (size_t i=0; i<g_battles_len; ++i)
		free(g_battles[i]);
	g_battles_len = 0;
	free(g_battles);
	g_battles = NULL;
}
