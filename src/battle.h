#pragma once

enum battleType {
	BATTLE_NORMAL = 0,
	BATTLE_REPLAY = 1,
};

enum natType {
	NAT_NONE  = 0,
	NAT_OTHER,
};

typedef struct Battle {
	//MUST BE FIRST SINCE IT IS USED BY HASH:
	uint32_t id;
	uint32_t mapHash;
	char mapName[MAX_TITLE+1], modName[MAX_TITLE+1], title[MAX_TITLE+1], ip[16];
	uint16_t port;
	uint8_t passworded, locked, type, natType, maxPlayers, rank, nbParticipants, nbSpectators, nbBots;
	union {
		union UserOrBot *users[128];
		User *founder;
	};
}Battle;

Battle * FindBattle(uint32_t id);
Battle *NewBattle(void);
void DelBattle(Battle *);
void ResetBattles(void);

#define FOR_EACH_PARTICIPANT(_u, _b)\
	for (UserOrBot **__u = (_b)->users, *(_u); (_u) = *__u, __u - (_b)->users < (_b)->nbParticipants; ++__u)

#define FOR_EACH_USER(_u, _b)\
	for (User **__u = (User **)(_b)->users, *(_u); (_u) = *__u, __u - (User **)(_b)->users < (_b)->nbParticipants - (_b)->nbBots; ++__u)

#define FOR_EACH_BOT(_u, _b)\
	for (User **__u = (Bot **)&(_b)->users[(_b)->nbBots], *(_u); (_u) = *__u, __u - (Bot **)(_b)->users < (_b)->nbParticipants; ++__u)

#define FOR_EACH_PLAYER(_u, _b)\
	for (UserOrBot **__u = (_b)->users, *(_u); (_u) = *__u, __u - (_b)->users < (_b)->nbParticipants; ++__u) if (!((_u)->battleStatus & MODE_MASK)) continue; else

#define FOR_EACH_HUMAN_PLAYER(_u, _b)\
	for (User **__u = (User **)(_b)->users, *(_u); (_u) = *__u, __u - (User **)(_b)->users < (_b)->nbParticipants - (_b)->nbBots; ++__u) if (!((_u)->battleStatus & MODE_MASK)) continue; else


#define GetNumPlayers(_b)\
	((_b)->nbParticipants - (_b)->nbSpectators - (_b)->nbBots)
