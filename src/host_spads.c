#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "chatbox.h"
#include "tasserver.h"
#include "common.h"
#include "host_spads.h"
#include "mybattle.h"
#include "spring.h"
#include "user.h"

static void relay_command (const char *format, ...) __attribute__ ((format (ms_printf, 1, 2)));

static void force_ally    (const User *, int ally);
static void force_team    (const User *, int team);
static void kick          (const UserBot *);
static void said_battle   (struct User *, char *text, ChatType);
static void set_map       (const char *map_name);
static void set_option    (Option *opt, const char *val);
static void set_split     (int size, SplitType type);
static void start_game    (void);

void
Spads_set_as_host(void)
{
	MyBattle_force_ally  = force_ally;
	MyBattle_force_team  = force_team;
	MyBattle_kick        = kick;
	MyBattle_said_battle = said_battle;
	MyBattle_set_map     = set_map;
	MyBattle_set_option  = set_option;
	MyBattle_set_split   = set_split;
	MyBattle_start_game  = start_game;
}

static void
force_ally(const User *user, int ally)
{
	relay_command("!force %s team %d",
			user->name, ally);
}

static void
force_team(const User *user, int team)
{
	relay_command("!force %s id %d",
			user->name, team);
}

static void
kick(const UserBot *u)
{
	relay_command("!kick %s", u->name);
}

static void
said_battle(User *user, char *text, ChatType chat_type)
{
	char ingame_name[MAX_NAME_LENGTH_NUL];

	if (user == g_my_battle->founder
	    && chat_type == CHAT_NORMAL
	    && sscanf(text, "<%" STRINGIFY(MAX_NAME_LENGTH_NUL) "[^>]> ", ingame_name) == 1) {
		text += 3 + strlen(ingame_name);
		BattleRoom_said_battle(ingame_name, text, CHAT_INGAME);
		return;
	}

	BattleRoom_said_battle(user->name, text, chat_type);
}

static void
set_map(const char *map_name)
{
	relay_command("!map %s",
	    map_name);
}

static void
set_option(Option *opt, const char *val)
{
	relay_command("!b_set %s %s",
	    opt->key, val);
}

static void
set_split(int size, SplitType type)
{
	if (STARTPOS_CHOOSE_INGAME != g_battle_options.start_pos_type)
		relay_command("!b_set startpostype 2");

	relay_command("!split %s %d",
			(char [][3]){"h", "v", "c1", "c2"}[type],
			size/2);
}

static void
start_game(void)
{
	if (g_my_battle->founder->ingame)
		Spring_launch();
	else
		relay_command("!start");
}

static void
relay_command(const char *format, ...)
{
	char buf[1024];
	va_list args;

	va_start (args, format);
	vsprintf(buf, format, args);
	va_end(args);

	TasServer_send_say_private(buf, false, g_my_battle->founder->name);
	g_last_auto_message = GetTickCount();
}
