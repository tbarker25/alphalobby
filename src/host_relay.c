#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "chatbox.h"
#include "tasserver.h"
#include "common.h"
#include "host_relay.h"
#include "mybattle.h"
#include "sync.h"
#include "user.h"

static void relay_command(const char *format, ...) __attribute__ ((format (ms_printf, 1, 2)));

static void force_ally          (const struct User *, int ally);
static void force_team          (const struct User *, int team);
static void kick                (const UserBot *);
static void set_map             (const char *map_name);
static void set_option          (Option *opt,         const char *val);
static void set_split           (int size,            SplitType type);
static void handle_manager_list (char *command);

void
Relay_set_as_host(void)
{
	MyBattle_force_ally  = force_ally;
	MyBattle_force_team  = force_team;
	MyBattle_kick        = kick;
	MyBattle_set_map     = set_map;
	MyBattle_set_option  = set_option;
	MyBattle_set_split   = set_split;
}

const char **g_relay_managers;
int g_relay_manager_len;

static char s_relay_cmd[1024];
static char s_relay_hoster[MAX_NAME_LENGTH_NUL];
static char s_relay_manager[MAX_NAME_LENGTH_NUL];
static char s_relay_password[1024];

bool
RelayHost_on_private_message(const char *username, char *command)
{
	// Get list of relayhost managers
	if (!strcmp(username, "RelayHostManagerList")) {
		handle_manager_list(command);
		return 1;
	}

	// If we are starting a relayhost game, then manager sends the name of the host to join:
	if (!strcmp(username, s_relay_manager)){
		strcpy(s_relay_hoster, command);
		*s_relay_manager = '\0';
		return 1;

	}

	// Relayhoster sending a users script password:
	if (!strcmp(username, s_relay_hoster) && !memcmp(command, "JOINEDBATTLE ", sizeof "JOINEDBATTLE " - 1)){
		/* command += sizeof "JOINEDBATTLE " - 1; */
		/* size_t len = strcspn(command, " "); */
		/* get_next_word(); */
		/* User *u = Users_find(get_next_word()); */
		/* if (u) { */
			/* free(u->script_password); */
			/* u->script_password = _strdup(get_next_word()); */
		/* } */
		return 1;
	}

	return 0;
}

static void
handle_manager_list(char *command)
{
	if (memcmp(command, "managerlist ", sizeof "managerlist " - 1)) {
		assert(0);
		return;
	}

	strsep(&command, " ");
	for (const char *c; (c = strsep(&command, " \t\n"));){
		User *u = Users_find(c);
		if (u) {
			g_relay_managers = realloc(g_relay_managers,
					((size_t)g_relay_manager_len+1) * sizeof *g_relay_managers);
			g_relay_managers[g_relay_manager_len++] = u->name;
		}
	}
}

void
RelayHost_open_battle(const char *title, const char *password, const char *mod_name, const char *map_name, const char *manager)
//OPENBATTLE type nat_type password port maxplayers hashcode rank maphash {map} {title} {modname}
{
	sprintf(s_relay_cmd, "!OPENBATTLE 0 0 %s 0 16 %d 0 %d %s\t%s\t%s", *password ? password : "*", Sync_mod_hash(mod_name), Sync_map_hash(map_name), map_name, title, mod_name);
	strcpy(s_relay_password, password ?: "*");
	strcpy(s_relay_manager, manager);
	TasServer_send_say_private("!spawn", 0, s_relay_manager);
}

void
RelayHost_on_add_user(const char *username)
{
	if (!strcmp(username, s_relay_hoster) && *s_relay_cmd) {
		/* TODO */
		/* TasServer_send(s_relay_cmd); */
		*s_relay_cmd = '\0';
	}
}

void
RelayHost_on_battle_opened(const Battle *b)
{
	if (!strcmp(b->founder->name, s_relay_hoster))
		TasServer_send_join_battle(b->id, s_relay_password);
}

/* void
g_relay_message() */
		/* { */
		/* extern char s_relay_hoster[1024]; */
		/* if (*s_relay_hoster) */
			/* command_start = sprintf(buf, "SAYPRIVATE %s ", */
					/* s_relay_hoster); */
		/* else */
			/* ++format; */
	/* } */


static void
force_ally(const User *user, int ally_id)
{
	relay_command("FORCEALLYNO %s %d", user->name, ally_id);
}

static void
force_team(const User *user, int team)
{
	relay_command("FORCETEAMNO %s %d", user->name, team);
}

/* static void
force_color(const char *name, uint32_t color)    */
/* {                                                           */
/*         relay_command("FORCETEAMCOLOR %s %d", name, color); */
/* }                                                           */

static void
kick(const UserBot *u)
{
	relay_command("KICKFROMBATTLE %s", u->name);
}

static void
set_map(const char *map_name)
{
	relay_command("UPDATEBATTLEINFO 0 0 %d %s",
			Sync_map_hash(map_name), map_name);
}

static void
set_option(Option *opt, const char *val)
{
	if (opt >= g_mod_options && opt < g_mod_options + g_mod_option_len)
		relay_command("SETSCRIPTTAGS game/modoptions/%s=%s", opt->key, val);

	else if (opt >= g_map_options && opt < g_map_options + g_map_option_len)
		relay_command("SETSCRIPTTAGS game/mapoptions/%s=%s", opt->key, val);

	else
		assert(0);

}

static void
add_start_box(int i, int left, int top, int right, int bottom)
{
	relay_command("ADDSTARTRECT %d %d %d %d %d", i, left, top, right, bottom);
}

static void
del_start_box(int i)
{
	relay_command("REMOVESTARTRECT %d", i);
}

static void
set_split(int size, SplitType type)
{
	switch (type) {
	case SPLIT_HORZ:
		add_start_box(0, 0, 0, size, 200);
		add_start_box(1, 200 - size, 0, 200, 200);
		del_start_box(2);
		del_start_box(3);
		return;

	case SPLIT_VERT:
		add_start_box(0, 0, 0, 200, size);
		add_start_box(1, 0, 200 - size, 200, 200);
		del_start_box(2);
		del_start_box(3);
		return;

	case SPLIT_CORNERS1:
		add_start_box(0, 0, 0, size, size);
		add_start_box(1, 200 - size, 200 - size, 200, 200);
		add_start_box(2, 0, 200 - size, size, 200);
		add_start_box(3, 200 - size, 0, 200, size);
		return;

	case SPLIT_CORNERS2:
		add_start_box(0, 0, 200 - size, size, 200);
		add_start_box(1, 200 - size, 0, 200, size);
		add_start_box(2, 0, 0, size, size);
		add_start_box(3, 200 - size, 200 - size, 200, 200);
		return;

	default:
		assert(0);
		return;
	}
}

static void
relay_command(const char *format, ...)
{
	char buf[1024];
	va_list args;

	va_start (args, format);
	vsprintf(buf, format, args);
	va_end(args);

	TasServer_send_say_private(buf, 0, s_relay_hoster);
}
