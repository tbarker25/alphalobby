#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "chat.h"
#include "client.h"
#include "client_message.h"
#include "common.h"
#include "host_relay.h"
#include "mybattle.h"
#include "sync.h"
#include "user.h"

static void force_ally(const char *name, int ally);
static void force_team(const char *name, int team);
static void kick(const char *name);
static void said_battle(const char *username, char *text);
static void set_map(const char *map_name);
static void set_option(Option *opt, const char *val);
static void set_split(int size, SplitType type);

static void handle_manager_list(char *command);

const HostType g_host_relay = {
	.force_ally = force_ally,
	.force_team = force_team,
	.kick = kick,
	.said_battle = said_battle,
	.set_map = set_map,
	.set_option = set_option,
	.set_split = set_split,
};

const char **g_relay_managers;
int g_relay_managersCount;

static char relayCmd[1024];
static char relayHoster[MAX_NAME_LENGTH_NUL];
static char relayManager[MAX_NAME_LENGTH_NUL];
static char relay_password[1024];

#define RelayMessage(format, ...) \
	(Server_send("SAYPRIVATE %s " format, relayHoster, ## __VA_ARGS__))

bool
RelayHost_on_private_message(const char *username, char *command)
{
	// Get list of relayhost managers
	if (!strcmp(username, "RelayHostManagerList")) {
		handle_manager_list(command);
		return true;
	}

	// If we are starting a relayhost game, then manager sends the name of the host to join:
	if (!strcmp(username, relayManager)){
		strcpy(relayHoster, command);
		*relayManager = '\0';
		return true;

	}

	// Relayhoster sending a users script password:
	if (!strcmp(username, relayHoster) && !memcmp(command, "JOINEDBATTLE ", sizeof("JOINEDBATTLE ") - 1)){
		/* command += sizeof("JOINEDBATTLE ") - 1; */
		/* size_t len = strcspn(command, " "); */
		/* getNextWord(); */
		/* User *u = Users_find(getNextWord()); */
		/* if (u) { */
			/* free(u->script_password); */
			/* u->script_password = _strdup(getNextWord()); */
		/* } */
		return true;
	}

	return false;
}

static void
handle_manager_list(char *command)
{
	if (memcmp(command, "managerlist ", sizeof("managerlist ") - 1)) {
		assert(0);
		return;
	}

	strsep(&command, " ");
	for (const char *c; (c = strsep(&command, " \t\n"));){
		User *u = Users_find(c);
		if (u) {
			g_relay_managers = realloc(g_relay_managers,
					(g_relay_managersCount+1) * sizeof(*g_relay_managers));
			g_relay_managers[g_relay_managersCount++] = u->name;
		}
	}
}

void
RelayHost_open_battle(const char *title, const char *password, const char *mod_name, const char *map_name, const char *manager)
//OPENBATTLE type nat_type password port maxplayers hashcode rank maphash {map} {title} {modname}
{
	sprintf(relayCmd, "!OPENBATTLE 0 0 %s 0 16 %d 0 %d %s\t%s\t%s", *password ? password : "*", Sync_mod_hash(mod_name), Sync_map_hash(map_name), map_name, title, mod_name);
	strcpy(relay_password, password ?: "*");
	strcpy(relayManager, manager);
	Server_send("SAYPRIVATE %s !spawn", relayManager);
}

void
RelayHost_on_add_user(const char *username)
{
	if (!strcmp(username, relayHoster) && *relayCmd) {
		Server_send(relayCmd);
		*relayCmd = '\0';
	}
}

void
RelayHost_on_battle_opened(const Battle *b)
{
	if (!strcmp(b->founder->name, relayHoster))
		JoinBattle(b->id, relay_password);
}

/* void
relayMessage() */
		/* { */
		/* extern char relayHoster[1024]; */
		/* if (*relayHoster) */
			/* command_start = sprintf(buf, "SAYPRIVATE %s ", */
					/* relayHoster); */
		/* else */
			/* ++format; */
	/* } */


static void
force_ally(const char *name, int ally_id)
{
	RelayMessage("FORCEALLYNO %s %d" , name, ally_id);
}

static void
force_team(const char *name, int team)
{
	RelayMessage("FORCETEAMNO %s %d" , name, team);
}

/* static void
forceColor(const char *name, uint32_t color)    */
/* {                                                           */
/*         RelayMessage("FORCETEAMCOLOR %s %d", name, color); */
/* }                                                           */

static void
kick(const char *name)
{
	RelayMessage("KICKFROMBATTLE %s", name);
}

static void
said_battle(const char *username, char *text)
{
	Chat_said(GetBattleChat(), username, 0, text);
}

static void
set_map(const char *map_name)
{
	RelayMessage("UPDATEBATTLEINFO 0 0 %d %s",
			Sync_map_hash(map_name), map_name);
}

static void
set_option(Option *opt, const char *val)
{
	if (opt >= g_mod_options && opt < g_mod_options + g_mod_option_count)
		RelayMessage("SETSCRIPTTAGS game/modoptions/%s=%s", opt->key, opt->val);

	else if (opt >= g_map_options && opt < g_map_options + g_map_option_count)
		RelayMessage("SETSCRIPTTAGS game/mapoptions/%s=%s", opt->key, opt->val);

	else
		assert(0);

}

static void
addStartBox(int i, int left, int top, int right, int bottom)
{
	RelayMessage("ADDSTARTRECT %d %d %d %d %d", i, left, top, right, bottom);
}

static void
delStartBox(int i)
{
	RelayMessage("REMOVESTARTRECT %d", i);
}

static void
set_split(int size, SplitType type)
{
	switch (type) {
	case SPLIT_HORZ:
		addStartBox(0, 0, 0, size, 200);
		addStartBox(1, 200 - size, 0, 200, 200);
		delStartBox(2);
		delStartBox(3);
		break;
	case SPLIT_VERT:
		addStartBox(0, 0, 0, 200, size);
		addStartBox(1, 0, 200 - size, 200, 200);
		delStartBox(2);
		delStartBox(3);
		break;
	case SPLIT_CORNERS1:
		addStartBox(0, 0, 0, size, size);
		addStartBox(1, 200 - size, 200 - size, 200, 200);
		addStartBox(2, 0, 200 - size, size, 200);
		addStartBox(3, 200 - size, 0, 200, size);
		break;
	case SPLIT_CORNERS2:
		addStartBox(0, 0, 200 - size, size, 200);
		addStartBox(1, 200 - size, 0, 200, size);
		addStartBox(2, 0, 0, size, size);
		addStartBox(3, 200 - size, 200 - size, 200, 200);
		break;
	default:
		assert(0);
		break;
	}
}
