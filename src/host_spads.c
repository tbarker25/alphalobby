#include <inttypes.h>
#include <stdio.h>
#include <windows.h>

#include "battleroom.h"
#include "battletools.h"
#include "chat.h"
#include "client_message.h"
#include "data.h"
#include "host_spads.h"

static void forceAlly(const char *name, int allyId);
static void forceTeam(const char *name, int teamId);
static void kick(const char *name);
static void saidBattle(const char *userName, char *text);
static void setMap(const char *mapName);
static void setOption(Option *opt, const char *val);
static void setSplit(int size, SplitType type);

#define SpadsMessageF(format, ...)\
	(gLastAutoMessage = GetTickCount(), SendToServer("SAYPRIVATE %s " format, gMyBattle->founder->name, ## __VA_ARGS__))

const HostType gHostSpads = {
	.forceAlly = forceAlly,
	.forceTeam = forceTeam,
	.kick = kick,
	.saidBattle = saidBattle,
	.setMap = setMap,
	.setOption = setOption,
	.setSplit = setSplit,
};

static void forceAlly(const char *name, int allyId)
{
	SpadsMessageF("!force %s team %d", name, allyId);
}

static void forceTeam(const char *name, int teamId)
{
	SpadsMessageF("!force %s id %d", name, teamId);
}

static void kick(const char *name)
{
	SpadsMessageF("!kick %s", name);
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
	SpadsMessageF("!map %s", mapName);
}

static void setOption(Option *opt, const char *val)
{
	SpadsMessageF("!bSet %s %s", opt->key, val);
}

static void setSplit(int size, SplitType type)
{
	size /= 2; //Script uses 200pts for each dimension, spads uses 100pts:
	if (STARTPOS_CHOOSE_INGAME != gBattleOptions.startPosType)
		SpadsMessageF("!bSet startpostype 2");
	SpadsMessageF("!split %s %d", (char [][3]){"h", "v", "c1", "c2"}[type], size);
}

