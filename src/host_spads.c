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
static void saidBattleEx(const char *userName, char *text);
static void setMap(const char *mapName);
static void setOption(Option *opt, const char *val);
static void setSplit(int size, SplitType type);
static void vote(int voteYes);

const HostType gHostSpads = {
	.forceAlly = forceAlly,
	.forceTeam = forceTeam,
	.kick = kick,
	.saidBattle = saidBattle,
	.saidBattleEx = saidBattleEx,
	.setMap = setMap,
	.setOption = setOption,
	.setSplit = setSplit,
	.vote = vote,
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

static void saidBattleEx(const char *userName, char *text)
{
	if (text[0] == '*' && text[1] == ' '){

		// Check for callvote:
		// "$user called a vote for command \"".join(" ",@{$p_params}."\" [!vote y, !vote n, !vote b]"
		if (!memcmp(" called a vote for command ", strchr(text + 2, ' ') ?: "", sizeof(" called a vote for command ") - 1)){
			char *commandStart = strchr(text, '"');
			if (commandStart){
				++commandStart;
				char *commandEnd = strchr(commandStart, '"');
				if (commandEnd){
					commandStart[0] = toupper(commandStart[0]);
					commandEnd[0] = '\0';
					BattleRoom_VoteStarted(commandStart);
					commandStart[0] = tolower(commandStart[0]);
					commandEnd[0] = '"';
				}
			}
		}

		// Check for endvote:
		if (!memcmp("* Vote for command \"", text, sizeof("* Vote for command \"") - 1)
				|| !memcmp("* Vote cancelled by ", text, sizeof("* Vote cancelled by ") - 1))
			BattleRoom_VoteEnded();
	}

	Chat_Said(GetBattleChat(), userName, CHAT_EX, text);
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

static void vote(int voteYes)
{
	SendToServer("SAYPRIVATE %s !vote %c",
			gMyBattle->founder, voteYes ? 'y' : 'n');
}
