#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <windows.h>

#include "battleroom.h"
#include "battletools.h"
#include "chat.h"
#include "client_message.h"
#include "data.h"
#include "host_spads.h"
#include "spring.h"

static void forceAlly(const char *name, int allyId);
static void forceTeam(const char *name, int teamId);
static void kick(const char *name);
static void saidBattle(const char *userName, char *text);
static void setMap(const char *mapName);
static void setOption(Option *opt, const char *val);
static void setSplit(int size, SplitType type);
static void startGame(void);

#define SendToServer(format, ...)\
	(gLastAutoMessage = GetTickCount(), SendToServer(format, ## __VA_ARGS__))

const HostType gHostSpads = {
	.forceAlly = forceAlly,
	.forceTeam = forceTeam,
	.kick = kick,
	.saidBattle = saidBattle,
	.setMap = setMap,
	.setOption = setOption,
	.setSplit = setSplit,
	.startGame = startGame,
};

static void forceAlly(const char *name, int allyId)
{
	SendToServer("SAYPRIVATE %s !force %s team %d",
			gMyBattle->founder->name, name, allyId);
}

static void forceTeam(const char *name, int teamId)
{
	SendToServer("SAYPRIVATE %s !force %s id %d",
			gMyBattle->founder->name, name, teamId);
}

static void kick(const char *name)
{
	SendToServer("SAYPRIVATE %s !kick %s",
			gMyBattle->founder->name, name);
}

static void saidBattle(const char *userName, char *text)
{
	char ingameUserName[MAX_NAME_LENGTH_NUL];

	if (!strcmp(userName, gMyBattle->founder->name)
			&& sscanf(text, "<%" STRINGIFY(MAX_NAME_LENGTH_NUL) "[^>]> ", ingameUserName) == 1) {
		text += 3 + strlen(ingameUserName);
		Chat_Said(GetBattleChat(), ingameUserName, CHAT_INGAME, text);
		return;
	}

	Chat_Said(GetBattleChat(), userName, 0, text);
}

static void setMap(const char *mapName)
{
	SendToServer("SAYPRIVATE %s !map %s",
			gMyBattle->founder->name, mapName);
}

static void setOption(Option *opt, const char *val)
{
	SendToServer("SAYPRIVATE %s !bSet %s %s",
			gMyBattle->founder->name, opt->key, val);
}

static void setSplit(int size, SplitType type)
{
	if (STARTPOS_CHOOSE_INGAME != gBattleOptions.startPosType)
		SendToServer("SAYPRIVATE %s !bSet startpostype 2",
				gMyBattle->founder->name);
	SendToServer("SAYPRIVATE %s !split %s %d",
			gMyBattle->founder->name,
			(char [][3]){"h", "v", "c1", "c2"}[type],
			size/2);
}

static void startGame(void)
{
	if (gMyBattle->founder->clientStatus & CS_INGAME_MASK)
		LaunchSpring();
	else
		SendToServer("SAYPRIVATE %s !start",
				gMyBattle->founder->name);
}
