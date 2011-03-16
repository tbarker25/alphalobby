#include "wincommon.h"
#include "data.h"
#include "client_message.h"
#include "dialogboxes.h"
#include "battletools.h"
#include "sync.h"
#include "settings.h"

static uint32_t balance;

static void addStartBox(int i, int left, int top, int right, int bottom)
{
	SendToServer("!ADDSTARTRECT %d %d %d %d %d", i, left, top, right, bottom);
	if (gBattleOptions.hostType == HOST_LOCAL)
		gBattleOptions.startRects[i] = (RECT){left, top, right, bottom};
}

static void delStartBox(int i)
{
	SendToServer("!REMOVESTARTRECT %d", i);
	if (gBattleOptions.hostType == HOST_LOCAL)
		gBattleOptions.startRects[i] = (RECT){0};
}


void SetSplit(SplitType type, int size)
{
	if (!gMyBattle)
		return;
	int startPosType = type < SPLIT_FIRST ? type : 2;
	
	if (gBattleOptions.hostType == HOST_SPADS) {
		if (startPosType != gBattleOptions.startPosType)
			SpadsMessageF("!bSet startpostype %d", startPosType);
		if (type >= SPLIT_FIRST)
			SpadsMessageF("!split %s %d", (char [][3]){"h", "v", "c1", "c2"}[type - SPLIT_FIRST], size);
		return;
	}
	
	//Script uses 200pts for each dimension, spads uses 100pts:
	size *= 2;
	
	if (startPosType != gBattleOptions.startPosType)
		ChangeOption(STARTPOS_FLAG | startPosType);

	switch (type) {
	case SPLIT_HORIZONTAL:
		addStartBox(0, 0, 0, size, 200);
		addStartBox(1, 200 - size, 0, 200, 200);
		delStartBox(2);
		delStartBox(3);
		break;
	case SPLIT_VERTICAL:
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
		RedrawMinimapBoxes();		
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
	if (!LoadSettingInt("host_autobalance") || gBattleOptions.hostType == HOST_SP)
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
	
	int balanceRank = LoadSettingInt("host_balance_rank");
	int balanceClan = LoadSettingInt("host_balance_clans");
	
	const int initialDiff = closest / -2;
	uint32_t seed = rand() % (1 << playerCount), n=seed;
	do {
		int rankDiff = initialDiff;
		for (int j=0; j<playerCount; ++j)
			if (n & 1<<j)
				rankDiff += 0x1000 + balanceRank * FROM_RANK_MASK(players[j]->battleStatus);
		rankDiff = abs(rankDiff);
		if (rankDiff > closest)
			goto failure;
		if (!balanceClan)
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
	int fixID = LoadSettingInt("host_fix_ids");
	int fixColor = LoadSettingInt("host_fix_colors");
	int fixAlly = LoadSettingInt("host_autobalance");
	
	FOR_EACH_PLAYER(p, gMyBattle) {
		++i;
		if (u && u != (void *)p)
			continue;

		uint32_t color = -1;
		if (fixColor) {
			color = ((i&0x01)*0xFF) | ((i&0x02)*0x7F80) | ((i&0x04)*0x3FC000);
			if (i & 0x08)
				color = i & 0x07 ? color & 0x010101 : 0x0c0c0c;
		}
		
		SetBattleStatusAndColor(p,
				TO_ALLY_MASK(!!(balance & 1<<i)) | TO_TEAM_MASK(i%16),
				fixAlly * ALLY_MASK | fixID * TEAM_MASK,
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
	return MODE_MASK | teamMask | TO_ALLY_MASK(ally > 0);
}

