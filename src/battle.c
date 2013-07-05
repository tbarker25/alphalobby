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

static int s_battles_len;
static Battle **s_battles;

Battle *
Battles_find(uint32_t id)
{
	for (int i=0; i<s_battles_len; ++i)
		if (s_battles[i] && s_battles[i]->id == id)
			return s_battles[i];
	return NULL;
}

Battle *
Battles_new(void)
{
	int i=0;

	for (; i<s_battles_len; ++i) {
		if (s_battles[i] == NULL)
			goto done;
	}

	if (s_battles_len % ALLOC_STEP == 0)
		s_battles = realloc(s_battles, (size_t)(s_battles_len + ALLOC_STEP) * sizeof *s_battles);
	++s_battles_len;
done:
	s_battles[i] = calloc(1, sizeof **s_battles);
	return s_battles[i];
}

void
Battles_del(Battle *b)
{
	free(b);
	for (int i=0; i<s_battles_len; ++i) {
		if (s_battles[i] == b) {
			s_battles[i] = NULL;
			return;
		}
	}
	assert(0);
}

void
Battles_reset(void)
{
	for (int i=0; i<s_battles_len; ++i)
		free(s_battles[i]);
	s_battles_len = 0;
	free(s_battles);
	s_battles = NULL;
}
