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
#include <unistd.h>

#include <windows.h>

#include "battle.h"
#include "common.h"
#include "mybattle.h"
#include "tsearch.h"
#include "user.h"

static int compare_int(const void *a, const void *b);

static void *s_battles;

Battle *
Battles_find(uint32_t id)
{
	return _tfind(&id, s_battles, compare_int);
}

Battle *
Battles_new(uint32_t id)
{
	Battle *b;

	b = _tinsert(&id, &s_battles, compare_int, sizeof *b);
	assert(!b->id);
	b->id = id;
	return b;
}

void
Battles_del(Battle *b)
{
	_tdelete(b, &s_battles, compare_int);
}

void
Battles_reset(void)
{
	_tdestroy(&s_battles);
}

static int
compare_int(const void *a, const void *b)
{
	return *(int32_t *)a - *(int32_t *)b;
}
