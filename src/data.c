#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "wincommon.h"

#include "data.h"
#include "sync.h"
#include "md5.h"
#include "settings.h"
#include "battlelist.h"
#include "common.h"
#include "client_message.h"
#include "battletools.h"
#include "battleroom.h"

extern uint32_t gLastBattleStatus;

User gMyUser[10000];
Battle *gMyBattle;
uint32_t gUdpHelpPort;


#define _users gMyUser
static size_t nbUsers = 1;

static Battle battles[10000];
static size_t nbBattles;

uint32_t battleToJoin;

uint32_t gMapHash, gModHash;
size_t gNbModOptions, gNbMapOptions;
Option *gModOptions, *gMapOptions;
BattleOption gBattleOptions;

char **gMaps, **gMods;
size_t gNbMaps, gNbMods;


MapInfo gMapInfo = {
	.description = (char[256]){},
	.author = (char[201]){},
};

Battle * FindBattle(uint32_t id)
{
	for (int i=0; i<nbBattles; ++i)
		if (battles[i].id == id)
			return &battles[i];
	return NULL;
}

User * FindUser(const char *name)
{
	for (int i=0; i<nbUsers; ++i)
		if (!strcmp(_users[i].name, name))
			return &_users[i];
	return NULL;
}

Battle *NewBattle(void)
{
	return &battles[nbBattles++];
}

void DelBattle(Battle *b)
{
	b->id = 0;
}

User *GetNextUser(void)
{
	static int last;
	return last < nbUsers ? &_users[last++] : (last = 0, NULL);
}

User * NewUser(uint32_t id, const char *name)
{
	if (!strcmp(gMyUser->name, name)) {
		gMyUser->id = id;
		return gMyUser;
	}

	int i=1;
	for (; i<nbUsers; ++i)
		if (_users[i].id == id)
			break;

	if (i == nbUsers) {
		++nbUsers;
		_users[i].id = id;
		strcpy(_users[i].alias, name);
	}
	SetWindowTextA(_users[i].chatWindow, name);
	return &_users[i];
}

void DelUser(User *u)
{
	u->name[0] = 0;
}

void ResetData (void)
{
	nbBattles = 0;
	for (int i=1; i < lengthof(_users); ++i)
		_users[i].name[0] = 0;

	memset(battles, 0, sizeof(battles));
	
	if (gMyBattle && gBattleOptions.hostType != HOST_SP)
		LeftBattle();
}

void AddBot(const char *name, User *owner, uint32_t battleStatus, uint32_t color, const char *aiDll)
{
	Bot *bot = calloc(1, sizeof(*bot));
	strcpy(bot->name, name);
	bot->owner = owner;
	bot->battleStatus = battleStatus | AI_MASK | MODE_MASK;
	bot->color = color;
	bot->dll = strdup(aiDll);
	
	Battle *b = gMyBattle;
	int i=b->nbParticipants - b->nbBots;
	while (i<b->nbParticipants && strcmpi(b->users[i]->name, bot->name) < 0)
		++i;
	for (int j=b->nbParticipants; j>i; --j)
		b->users[j] = b->users[j-1];
	b->users[i] = (UserOrBot *)bot;
	++b->nbParticipants;
	++b->nbBots;
	
	Rebalance();
	SendMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
	BattleRoom_UpdateUser((void *)bot);
	
	if (gBattleOptions.hostType == HOST_SP) {
		bot->nbOptions = UnitSync_GetSkirmishAIOptionCount(aiDll);
		bot->options = calloc(bot->nbOptions, sizeof(*bot->options));
		UnitSync_GetOptions(bot->options, bot->nbOptions);
	}
}

void DelBot(const char *name)
{
	int i = gMyBattle->nbParticipants - gMyBattle->nbBots;
	while (i < gMyBattle->nbParticipants && strcmpi(gMyBattle->users[i]->name, name) < 0)
		++i;
	BattleRoom_RemoveUser(gMyBattle->users[i]);
	free(gMyBattle->users[i]->bot.dll);
	free(gMyBattle->users[i]->bot.options);
	free(gMyBattle->users[i]);
	while (++i < gMyBattle->nbParticipants)
		gMyBattle->users[i - 1] = gMyBattle->users[i];
	--gMyBattle->nbParticipants;
	--gMyBattle->nbBots;
	Rebalance();
	SendMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
}

void LeftBattle(void)
{
	BattleRoom_Hide();
	
	while (gMyBattle->nbBots)
		DelBot(gMyBattle->users[gMyBattle->nbParticipants - 1]->bot.name);
	
	if (gBattleOptions.hostType == HOST_SP)
		free(gMyBattle);

	gMyUser->battleStatus = LOCK_BS_MASK;
	gLastBattleStatus = LOCK_BS_MASK;
	
	gMyBattle = NULL;
	if (battleToJoin)
		JoinBattle(battleToJoin, NULL);
}

void ResetBattleOptions(void)
{
	memset(&gBattleOptions, 0, sizeof(gBattleOptions));
	for (int i=0; i<lengthof(gBattleOptions.positions); ++i)
		gBattleOptions.positions[i] = INVALID_STARTPOS;

}
void JoinedBattle(Battle *b, uint32_t modHash)
{
	gMyBattle = b;
	ResetBattleOptions();
	gBattleOptions.modHash = modHash;

	if (!strcmp(b->founder->name, relayHoster)) {
		gBattleOptions.hostType = HOST_RELAY;
		SendToServer("!SUPPORTSCRIPTPASSWORD");
	} else if (b->founder ==  gMyUser)
		gBattleOptions.hostType = HOST_LOCAL;

	gMyUser->battleStatus = 0;
	gLastBattleStatus = LOCK_BS_MASK;

	if (gModHash !=modHash)
		ChangeMod(b->modName);
	if (gMapHash != b->mapHash)
		ChangeMap(b->mapName);

	BattleRoom_Show();
}

void UpdateBattleStatus(UserOrBot *s, uint32_t bs, uint32_t color)
{
	uint32_t lastBS = s->battleStatus;
	s->battleStatus = bs;
	uint32_t lastColor = s->color;
	s->color = color;
	
	#ifdef NDEBUG
	if (!s || !gMyBattle || gMyBattle != s->battle)
		return;
	#endif
	
	BattleRoom_UpdateUser(s);
	
	if (&s->user == gMyUser)
		gLastBattleStatus = bs;
	
	if ((lastBS ^ bs) & MODE_MASK) {
		gMyBattle->nbSpectators = 0;
		for (int i=0; i < gMyBattle->nbParticipants; ++i)
			gMyBattle->nbSpectators += !(gMyBattle->users[i]->battleStatus & MODE_MASK);
		BattleList_UpdateBattle(gMyBattle);
		Rebalance();
	} else if (bs & MODE_MASK
			&& ((lastBS ^ bs) & (TEAM_MASK | ALLY_MASK)
			   ||  lastColor != color)) {
		if (gBattleOptions.hostType != HOST_SP)
			FixPlayerStatus((void *)s);
	} else
		return;
	
	SendMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);

	if (&s->user == gMyUser && (lastBS ^ bs) & (MODE_MASK | ALLY_MASK))
		RedrawMinimapBoxes();

}
