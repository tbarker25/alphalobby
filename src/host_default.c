
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
#include <stdio.h>
#include <windows.h>

#include "battleroom.h"
#include "battletools.h"
#include "chat.h"
#include "client_message.h"
#include "data.h"
#include "host_default.h"

static void saidBattle(const char *userName, char *text);
static void saidBattleEx(const char *userName, char *text);

const HostType gHostDefault = {
	.saidBattle = saidBattle,
	.saidBattleEx = saidBattleEx,
};

static void saidBattleEx(const char *userName, char *text)
{
	Chat_Said(GetBattleChat(), userName, CHAT_EX, text);
}

static void saidBattle(const char *userName, char *text)
{
	Chat_Said(GetBattleChat(), userName, 0, text);
}
