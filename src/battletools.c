#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>

#include <windows.h>

#include "alphalobby.h"
#include "battlelist.h"
#include "battleroom.h"
#include "battletools.h"
#include "client_message.h"
#include "data.h"
#include "dialogboxes.h"
#include "settings.h"
#include "sync.h"

static uint32_t balance;
uint32_t gUdpHelpPort;

uint32_t battleToJoin;

Battle *gMyBattle;
extern uint32_t gLastBattleStatus;

uint32_t gMapHash, gModHash;
ssize_t gNbModOptions, gNbMapOptions;
Option *gModOptions, *gMapOptions;
BattleOption gBattleOptions;

uint8_t gNbSides;
char gSideNames[16][32];

char **gMaps, **gMods;
ssize_t gNbMaps = -1, gNbMods = -1;

static char *currentScript;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

struct _LargeMapInfo _gLargeMapInfo = {.mapInfo = {.description = _gLargeMapInfo.description, .author = _gLargeMapInfo.author}};

static void addStartBox(int i, int left, int top, int right, int bottom)
{
	SendToServer("!ADDSTARTRECT %d %d %d %d %d", i, left, top, right, bottom);
	if (gBattleOptions.hostType == HOST_LOCAL)
		gBattleOptions.startRects[i] = (StartRect){left, top, right, bottom};
}

static void delStartBox(int i)
{
	SendToServer("!REMOVESTARTRECT %d", i);
	if (gBattleOptions.hostType == HOST_LOCAL)
		gBattleOptions.startRects[i] = (StartRect){0};
}


void SetSplit(SplitType type, int size)
{
	if (!gMyBattle)
		return;
	// int startPosType = type < SPLIT_FIRST ? type : 2;
	int startPosType = 2;
	
	if (gBattleOptions.hostType == HOST_SPADS) {
		size /= 2; //Script uses 200pts for each dimension, spads uses 100pts:
		if (startPosType != gBattleOptions.startPosType)
			SpadsMessageF("!bSet startpostype %d", startPosType);
		SpadsMessageF("!split %s %d", (char [][3]){"h", "v", "c1", "c2"}[type], size);
		return;
	}
	

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
		addStartBox(1, 200-size, 200-size, 200, 200);
		addStartBox(2, 0, 200-size, size, 200);
		addStartBox(3, 200-size, 0, 200, size);
		break;
	case SPLIT_CORNERS2:
		addStartBox(0, 0, 200-size, size, 200);
		addStartBox(1, 200-size, 0, 200, size);
		addStartBox(2, 0, 0, size, size);
		addStartBox(3, 200-size, 200-size, 200, 200);
		break;
	default:
		return;
	}
	if (gBattleOptions.hostType == HOST_LOCAL)
		;
		/* RedrawMinimapBoxes();		 */
}

void SetMap(const char *name)
{
	if (gBattleOptions.hostType == HOST_SPADS)
		SpadsMessageF("!map %s", name);
	else if (gBattleOptions.hostType == HOST_RELAY)
		SendToServer("!UPDATEBATTLEINFO %d %d %u %s", gMyBattle->nbSpectators, gMyBattle->locked, gMyBattle->mapHash, gMyBattle->mapName);
}

void Rebalance(void)
//NB: this function is cpu expensive
{
	if (!LoadSettingInt("host_autobalance"))
		return;
	
	int playerCount = gMyBattle->nbParticipants - gMyBattle->nbSpectators;
	const union UserOrBot *players[playerCount];
	
	int i=0;
	FOR_EACH_PLAYER(p, gMyBattle)
		players[i++] = (void *)p;

	uint32_t closestN = 0;
	int closest = 0x1000 * playerCount;
	for (int j=0; j<playerCount; ++j)
		closest += FROM_RANK_MASK(players[j]->battleStatus);
	
	const int initialDiff = closest / -2;
	uint32_t seed = rand() % (1 << playerCount), n=seed;
	do {
		int rankDiff = initialDiff;
		for (int j=0; j<playerCount; ++j)
			if (n & 1<<j)
				rankDiff += 0x1000 + ((gSettings.flags & SETTING_HOST_BALANCE_RANK) != 0) * FROM_RANK_MASK(players[j]->battleStatus);
		rankDiff = abs(rankDiff);
		if (rankDiff > closest)
			goto failure;
		if (!((gSettings.flags & SETTING_HOST_BALANCE_CLAN) != 0))
			goto success;
		for (int i=0; i<playerCount-1; ++i) {
			if (players[i]->name[0] != '[')
				continue;
			const char *tagEnd = strchr(players[i]->name, ']');
			if (!tagEnd)
				continue;
			size_t tagLength = tagEnd - players[i]->name + 1; //include '[' and ']'
			for (int j=i+1; j<playerCount; ++j)
				if (!(1<<i & n) != !(1<<j & n) && !strncmp(players[i]->name, players[j]->name, tagLength))
					goto failure;
		}
		success:
		closestN = n;
		if (!(closest = rankDiff)) //Can't beat perfect
			break;
		failure:
		n = (n + 1) % (1 << playerCount);
	} while (n != seed);
	// puts("team 1");
	// int ranks[2] = {};
	// for (int i=0; i<playerCount; ++i) {
		// if (closestN & 1 << i) {
			// puts(players[i]->name);
			// ranks[0] += balanceRank * FROM_RANK_MASK(players[i]->battleStatus);
		// }
	// }
	// printf("total ranks = %d\n\n", ranks[0]);
	// puts("team 2");
	// for (int i=0; i<playerCount; ++i) {
		// if (!(closestN & 1 << i)) {
			// puts(players[i]->name);
			// ranks[1] += FROM_RANK_MASK(players[i]->battleStatus);
		// }
	// }
	// printf("total ranks = %d\n\n", ranks[1]);	
			
	balance = closestN;
	FixPlayerStatus(NULL);
}

void FixPlayerStatus(const union UserOrBot *u)
{
	if (!(gBattleOptions.hostType & HOST_FLAG))
		return;

	int i=-1;
	
	FOR_EACH_PLAYER(p, gMyBattle) {
		++i;
		if (u && u != (void *)p)
			continue;

		uint32_t color = -1;
		if (((gSettings.flags & SETTING_HOST_FIX_COLOR) != 0)) {
			color = ((i&0x01)*0xFF) | ((i&0x02)*0x7F80) | ((i&0x04)*0x3FC000);
			if (i & 0x08)
				color = i & 0x07 ? color & 0x010101 : 0x0c0c0c;
		}
		
		SetBattleStatusAndColor(p,
				TO_ALLY_MASK(!!(balance & 1<<i)) | TO_TEAM_MASK(i%16),
				((gSettings.flags & SETTING_HOST_BALANCE) != 0) * ALLY_MASK | ((gSettings.flags & SETTING_HOST_FIX_ID) != 0) * TEAM_MASK,
				color);
	}
}

uint32_t GetNewBattleStatus(void)
{
	int teamBitField=0, ally=0;
	FOR_EACH_PLAYER(s, gMyBattle) {
		ally += ((s->battleStatus & ALLY_MASK) == 0) - ((s->battleStatus & ALLY_MASK) == TO_ALLY_MASK(1));
		teamBitField |= 1 << FROM_TEAM_MASK(s->battleStatus);
	}
	int teamMask = 0;
	for (int i=0; i<16; ++i) {
		if (!(teamBitField & 1 << i)) {
			teamMask = TO_TEAM_MASK(i);
			break;
		}
	}
	return MODE_MASK | READY_MASK | teamMask | TO_ALLY_MASK(ally > 0);
}

void JoinedBattle(Battle *b, uint32_t modHash)
{
	gMyBattle = b;
	gBattleOptions.modHash = modHash;

	if (!strcmp(b->founder->name, relayHoster)) {
		gBattleOptions.hostType = HOST_RELAY;
		SendToServer("!SUPPORTSCRIPTPASSWORD");
	} else if (b->founder ==  &gMyUser)
		gBattleOptions.hostType = HOST_LOCAL;

	gMyUser.battleStatus = 0;
	gLastBattleStatus = 0;

	if (gModHash !=modHash)
		ChangedMod(b->modName);
	if (gMapHash != b->mapHash || !gMapHash)
		ChangedMap(b->mapName);

	BattleRoom_Show();
}

void LeftBattle(void)
{
	BattleRoom_Hide();

	memset(&gBattleOptions, 0, sizeof(gBattleOptions));
	for (int i=0; i<LENGTH(gBattleOptions.positions); ++i)
		gBattleOptions.positions[i] = INVALID_STARTPOS;

	free(currentScript);
	currentScript = NULL;

	BattleRoom_OnSetModDetails();
	
	while (gMyBattle->nbBots)
		DelBot(gMyBattle->users[gMyBattle->nbParticipants - 1]->bot.name);
	
	gMyUser.battleStatus = 0;
	gLastBattleStatus = 0;
	battleInfoFinished = 0;
	
	gMyBattle = NULL;
	if (battleToJoin)
		JoinBattle(battleToJoin, NULL);

	*relayHoster = '\0';
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
		FixPlayerStatus((void *)s);
	} else
		return;
	if ((lastBS ^ bs) & MODE_MASK
			|| ((lastBS ^ bs) & ALLY_MASK && &s->user == &gMyUser))
		BattleRoom_StartPositionsChanged();
}

void ChangeOption(Option *opt)
{
	const char *path;
	if (opt >= gModOptions && opt < gModOptions + gNbModOptions)
		path = "modoptions/";
	else if (opt >= gMapOptions && opt < gMapOptions + gNbMapOptions)
		path = "mapoptions/";
	else
		assert(0);

	switch (opt->type) {
		char *tmp;
		HMENU menu;
	case opt_bool:
		opt->val = (char [2]){opt->val[0] ^ ('0' ^ '1')};
		break;
	case opt_number:
		tmp = alloca(128);
		tmp[0] = '\0';
		if (GetTextDlg(opt->name, tmp, 128))
			return;
		opt->val = tmp;
		break;
	case opt_list:
		menu = CreatePopupMenu();
		for (int j=0; j < opt->nbListItems; ++j)
			AppendMenuA(menu, 0, j+1, opt->listItems[j].name);
		SetLastError(0);
		POINT point;
		GetCursorPos(&point);
		void func(int *i) {
			*i = TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x, point.y, gMainWindow, NULL);
		}
		int clicked;
		SendMessage(gMainWindow, WM_EXECFUNCPARAM, (WPARAM)func, (LPARAM)&clicked);
		if (!clicked)
			return;
		opt->val = strcpy(alloca(128), opt->listItems[clicked - 1].key);
		DestroyMenu(menu);
		break;
	default:
		return;
	}

	if (gBattleOptions.hostType == HOST_SPADS) {
		SpadsMessageF("!bSet %s %s", opt->key, opt->val);
	} else if (gBattleOptions.hostType & HOST_FLAG) {
		SendToServer("!SETSCRIPTTAGS game/%s%s=%s", path ?: "", opt->key, opt->val);
	}
}

static void setScriptTag(const char *key, const char *val)
{
	if (!_strnicmp(key, "game/startpostype", sizeof("game/startpostype") - 1)) {
		StartPosType startPosType = atoi(val);
		if (startPosType != gBattleOptions.startPosType)
			;// taskSetMinimap = 1;
		gBattleOptions.startPosType = startPosType;
		// PostMessage(gBattleRoom, WM_MOVESTARTPOSITIONS, 0, 0);
		return;
	}
	
	if (!_strnicmp(key, "game/team", sizeof("game/team") - 1)) {
		int team = atoi(key + sizeof("game/team") - 1);
		char type = key[sizeof("game/team/startpos") + (team > 10)];
		((int *)&gBattleOptions.positions[team])[type != 'x'] = atoi(val);
		// PostMessage(gBattleRoom, WM_MOVESTARTPOSITIONS, 0, 0);
		return;
	}
	
	size_t nbOptions;
	Option *options;
	if (!_strnicmp(key, "game/modoptions/", sizeof("game/modoptions/") - 1)) {
		options = gModOptions;
		nbOptions = gNbModOptions;
	} else if (!_strnicmp(key, "game/mapoptions/", sizeof("game/mapoptions/") - 1)) {
		options = gMapOptions;
		nbOptions = gNbMapOptions;
	} else {
		printf("unrecognized script key %s=%s\n", key, val);
		return;
	}

	key += sizeof("game/modoptions/") - 1;
	for (int i=0; i<nbOptions; ++i) {
		if (strcmp(options[i].key, key))
			continue;
		free(options[i].val);
		options[i].val = strdup(val);
		BattleRoom_OnSetOption(&options[i]);
	}
}

void SetScriptTags(char *script)
{
	if (!currentScript)
		currentScript = strdup(script);
	else {
		size_t currentLen = strlen(currentScript) + 1;
		size_t extraLen = strlen(script) + 1;
		currentScript = realloc(currentScript,
				currentLen + extraLen);
		currentScript[currentLen - 1] = '\t';
		memcpy(currentScript + currentLen, script, extraLen);
	}

	if (!gModHash)
		return;

	char *key, *val;
	while ((key = strsep(&script, "=")) && (val = strsep(&script, "\t")))
		setScriptTag(key, val);
}

void UpdateModOptions(void)
{
	BattleRoom_OnSetModDetails();

	if (!currentScript)
		return;

	char *key, *val;
	char *script = strdup(currentScript);
	while ((key = strsep(&script, "=")) && (val = strsep(&script, "\t")))
		setScriptTag(key, val);
	free(script);

	/* Set default values */
	for (int i=0; i<gNbModOptions; ++i)
		if (!gModOptions[i].val)
			BattleRoom_OnSetOption(&gModOptions[i]);
	for (int i=0; i<gNbMapOptions; ++i)
		if (!gMapOptions[i].val)
			BattleRoom_OnSetOption(&gMapOptions[i]);
}

