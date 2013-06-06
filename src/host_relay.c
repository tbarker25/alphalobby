#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data.h"
#include "client.h"
#include "client_message.h"
#include "common.h"
#include "host_relay.h"
#include "sync.h"
#include "user.h"

const char **gRelayManagers;
int gRelayManagersCount;

static char relayCmd[1024];
char relayHoster[1024];
static char relayManager[1024];
static char relayPassword[1024];

static void handleManagerList(char *command);

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
