#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "chat.h"
#include "client.h"
#include "client_message.h"
#include "common.h"
#include "host_spads.h"
#include "mybattle.h"
#include "spring.h"
#include "user.h"

#define Server_send(format, ...)\
	(g_last_auto_message = GetTickCount(), Server_send(format, ## __VA_ARGS__))

static void force_ally(const char *name, int ally);
static void force_team(const char *name, int team);
static void kick(const char *name);
static void said_battle(const char *username, char *text);
static void set_map(const char *map_name);
static void set_option(Option *opt, const char *val);
static void set_split(int size, SplitType type);
static void start_game(void);

const HostType HOST_SPADS = {
	.force_ally = force_ally,
	.force_team = force_team,
	.kick = kick,
	.said_battle = said_battle,
	.set_map = set_map,
	.set_option = set_option,
	.set_split = set_split,
	.start_game = start_game,
};

static void
force_ally(const char *name, int ally)
{
	Server_send("SAYPRIVATE %s !force %s team %d",
			g_my_battle->founder->name, name, ally);
}

static void
force_team(const char *name, int team)
{
	Server_send("SAYPRIVATE %s !force %s id %d",
			g_my_battle->founder->name, name, team);
}

static void
kick(const char *name)
{
	Server_send("SAYPRIVATE %s !kick %s",
			g_my_battle->founder->name, name);
}

static void
said_battle(const char *username, char *text)
{
	char ingame_name[MAX_NAME_LENGTH_NUL];

	if (!strcmp(username, g_my_battle->founder->name)
			&& sscanf(text, "<%" STRINGIFY(MAX_NAME_LENGTH_NUL) "[^>]> ", ingame_name) == 1) {
		text += 3 + strlen(ingame_name);
		Chat_said(GetBattleChat(), ingame_name, CHAT_INGAME, text);
		return;
	}

	Chat_said(GetBattleChat(), username, 0, text);
}

static void
set_map(const char *map_name)
{
	Server_send("SAYPRIVATE %s !map %s",
			g_my_battle->founder->name, map_name);
}

static void
set_option(Option *opt, const char *val)
{
	Server_send("SAYPRIVATE %s !b_set %s %s",
			g_my_battle->founder->name, opt->key, val);
}

static void
set_split(int size, SplitType type)
{
	if (STARTPOS_CHOOSE_INGAME != g_battle_options.start_pos_type)
		Server_send("SAYPRIVATE %s !b_set startpostype 2",
				g_my_battle->founder->name);
	Server_send("SAYPRIVATE %s !split %s %d",
			g_my_battle->founder->name,
			(char [][3]){"h", "v", "c1", "c2"}[type],
			size/2);
}

static void
start_game(void)
{
	if (g_my_battle->founder->ingame)
		Spring_launch();
	else
		Server_send("SAYPRIVATE %s !start",
				g_my_battle->founder->name);
}
