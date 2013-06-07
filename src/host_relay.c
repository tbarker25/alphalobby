#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "battleroom.h"
#include "data.h"
#include "chat.h"
#include "client.h"
#include "client_message.h"
#include "common.h"
#include "host_relay.h"
#include "sync.h"
#include "user.h"

static void forceAlly(const char *name, int allyId);
static void forceTeam(const char *name, int teamId);
static void kick(const char *name);
static void saidBattle(const char *userName, char *text);
static void setMap(const char *mapName);
static void setOption(Option *opt, const char *val);
static void setSplit(int size, SplitType type);

static void handleManagerList(char *command);

const HostType gHostRelay = {
	.forceAlly = forceAlly,
	.forceTeam = forceTeam,
	.kick = kick,
	.saidBattle = saidBattle,
	.setMap = setMap,
	.setOption = setOption,
	.setSplit = setSplit,
};

const char **gRelayManagers;
int gRelayManagersCount;

static char relayCmd[1024];
static char relayHoster[1024];
static char relayManager[1024];
static char relayPassword[1024];

#define RelayMessage(format, ...) \
	(SendToServer("SAYPRIVATE %s " format, relayHoster, ## __VA_ARGS__))

bool RelayHost_handlePrivateMessage(const char *username, char *command)
{
	// Get list of relayhost managers
	if (!strcmp(username, "RelayHostManagerList")) {
		handleManagerList(command);
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
		/* User *u = FindUser(getNextWord()); */
		/* if (u) { */
			/* free(u->scriptPassword); */
			/* u->scriptPassword = strdup(getNextWord()); */
		/* } */
		return true;
	}

	return false;
}

static void handleManagerList(char *command)
{
	if (memcmp(command, "managerlist ", sizeof("managerlist ") - 1)) {
		assert(0);
		return;
	}

	strsep(&command, " ");
	for (const char *c; (c = strsep(&command, " \t\n"));){
		User *u = FindUser(c);
		if (u) {
			gRelayManagers = realloc(gRelayManagers,
					(gRelayManagersCount+1) * sizeof(*gRelayManagers));
			gRelayManagers[gRelayManagersCount++] = u->name;
		}
	}
}

void OpenRelayBattle(const char *title, const char *password, const char *modName, const char *mapName, const char *manager)
//OPENBATTLE type natType password port maxplayers hashcode rank maphash {map} {title} {modname}
{
	sprintf(relayCmd, "!OPENBATTLE 0 0 %s 0 16 %d 0 %d %s\t%s\t%s", *password ? password : "*", GetModHash(modName), GetMapHash(mapName), mapName, title, modName);
	strcpy(relayPassword, password ?: "*");
	strcpy(relayManager, manager);
	SendToServer("SAYPRIVATE %s !spawn", relayManager);
}

void RelayHost_onAddUser(const char *username)
{
	if (!strcmp(username, relayHoster) && *relayCmd) {
		SendToServer(relayCmd);
		*relayCmd = '\0';
	}
}

void RelayHost_onBattleOpened(const Battle *b)
{
	if (!strcmp(b->founder->name, relayHoster))
		JoinBattle(b->id, relayPassword);
}

/* void relayMessage() */
		/* { */
		/* extern char relayHoster[1024]; */
		/* if (*relayHoster) */
			/* commandStart = sprintf(buff, "SAYPRIVATE %s ", */
					/* relayHoster); */
		/* else */
			/* ++format; */
	/* } */


static void forceAlly(const char *name, int allyId)
{
	RelayMessage("FORCEALLYNO %s %d" , name, allyId);
}

static void forceTeam(const char *name, int teamId)
{
	RelayMessage("FORCETEAMNO %s %d" , name, teamId);
}

/* static void forceColor(const char *name, uint32_t color)    */
/* {                                                           */
/*         RelayMessage("FORCETEAMCOLOR %s %d", name, color); */
/* }                                                           */

static void kick(const char *name)
{
	RelayMessage("KICKFROMBATTLE %s", name);
}

static void saidBattle(const char *userName, char *text)
{
	Chat_Said(GetBattleChat(), userName, 0, text);
}

static void setMap(const char *mapName)
{
	RelayMessage("UPDATEBATTLEINFO 0 0 %d %s",
			GetMapHash(mapName), mapName);
}

static void setOption(Option *opt, const char *val)
{
	const char *path;
	if (opt >= gModOptions && opt < gModOptions + gNbModOptions)
		path = "modoptions/";
	else if (opt >= gMapOptions && opt < gMapOptions + gNbMapOptions)
		path = "mapoptions/";
	else
		assert(0);

	RelayMessage("SETSCRIPTTAGS game/%s%s=%s", path ?: "", opt->key, opt->val);
}

static void addStartBox(int i, int left, int top, int right, int bottom)
{
	RelayMessage("ADDSTARTRECT %d %d %d %d %d", i, left, top, right, bottom);
}

static void delStartBox(int i)
{
	RelayMessage("REMOVESTARTRECT %d", i);
}

static void setSplit(int size, SplitType type)
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
