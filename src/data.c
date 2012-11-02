#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <windows.h>
#include <windowsx.h>
#include <oleacc.h>
#include <Commctrl.h>
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

#define ALLOC_STEP 10

static Battle **battles;
Battle *gMyBattle;
static size_t nbBattles;

uint32_t gUdpHelpPort;

uint32_t battleToJoin;

extern uint32_t gLastBattleStatus;

uint32_t gMapHash, gModHash;
size_t gNbModOptions, gNbMapOptions;
Option *gModOptions, *gMapOptions;
BattleOption gBattleOptions;

uint8_t gNbSides;
char gSideNames[16][32];

char **gMaps, **gMods;
ssize_t gNbMaps = -1, gNbMods = -1;

struct _LargeMapInfo _gLargeMapInfo = {.mapInfo = {.description = _gLargeMapInfo.description, .author = _gLargeMapInfo.author}};

Battle * FindBattle(uint32_t id)
{
	for (int i=0; i<nbBattles; ++i)
		if (battles[i] && battles[i]->id == id)
			return battles[i];
	return NULL;
}

Battle *NewBattle(void)
{
	int i=0;
	for (; i<nbBattles; ++i) {
		if (battles[i] == NULL)
			break;
	}
	if (i == nbBattles) {
		if (nbBattles % ALLOC_STEP == 0)
			battles = realloc(battles, (nbBattles + ALLOC_STEP) * sizeof(Battle *));
		++nbBattles;
	}
	battles[i] = calloc(1, sizeof(Battle));
	return battles[i];
}

void DelBattle(Battle *b)
{
	free(b);
	b = NULL;
}


void ResetData (void)
{
	nbBattles = 0;
	/* for (int i=0; i<nbUsers; ++i) { */
		/* users[i]->name[0] = 0; */
	/* } */
	for (int i=0; i<nbBattles; ++i) {
		free(battles[i]);
		battles[i] = NULL;
	}
	
	if (gMyBattle && gBattleOptions.hostType != HOST_SP)
		LeftBattle();
	
	BattleList_Reset();
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
	BattleRoom_StartPositionsChanged();
	BattleRoom_UpdateUser((void *)bot);
	
	if (gBattleOptions.hostType == HOST_SP) {
		// bot->nbOptions = UnitSync_GetSkirmishAIOptionCount(aiDll);
		// bot->options = calloc(bot->nbOptions, sizeof(*bot->options));
		// UnitSync_GetOptions(bot->options, bot->nbOptions);
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
	BattleRoom_StartPositionsChanged();
}

void LeftBattle(void)
{
	BattleRoom_Hide();
	
	while (gMyBattle->nbBots)
		DelBot(gMyBattle->users[gMyBattle->nbParticipants - 1]->bot.name);
	
	if (gBattleOptions.hostType == HOST_SP)
		free(gMyBattle);

	gMyUser.battleStatus = LOCK_BS_MASK;
	gLastBattleStatus = LOCK_BS_MASK;
	
	gMyBattle = NULL;
	if (battleToJoin)
		JoinBattle(battleToJoin, NULL);

	*relayHoster = '\0';
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
	} else if (b->founder ==  &gMyUser)
		gBattleOptions.hostType = HOST_LOCAL;

	gMyUser.battleStatus = 0;
	gLastBattleStatus = LOCK_BS_MASK;

	if (gModHash !=modHash)
		ChangedMod(b->modName);
	if (gMapHash != b->mapHash || !gMapHash)
		ChangedMap(b->mapName);

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
	
	if (&s->user == &gMyUser)
		gLastBattleStatus = bs;
	
	if ((lastBS ^ bs) & MODE_MASK) {
		if (!(bs & MODE_MASK) && BattleRoom_IsAutoUnspec())
			SetBattleStatus(&gMyUser, MODE_MASK, MODE_MASK);
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
	BattleRoom_StartPositionsChanged();

	if (&s->user == &gMyUser && (lastBS ^ bs) & (MODE_MASK | ALLY_MASK))
		BattleRoom_StartPositionsChanged();

}
