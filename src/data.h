#pragma once

#include "common.h"
#include <inttypes.h>
#include "unitsync.h" //for mapInfo

enum battleType {
	BATTLE_NORMAL = 0,
	BATTLE_REPLAY = 1,
};

enum natType {
	NAT_NONE  = 0,
	NAT_OTHER,
};

enum modeType {
	MODE_SPECTATOR = 0,
	MODE_PLAYER = 1,
};

#define READY_OFFSET 1
#define TEAM_OFFSET 2
#define ALLY_OFFSET 6
#define MODE_OFFSET 10
#define HANDICAP_OFFSET 11
#define SYNC_OFFSET 22
#define SIDE_OFFSET 24
#define LOCK_BS_OFFSET 30
#define AI_OFFSET 31
enum {
	READY_MASK    = 0x01 << READY_OFFSET,
	TEAM_MASK     = 0x0F << TEAM_OFFSET,
	ALLY_MASK     = 0x0F << ALLY_OFFSET,
	MODE_MASK     = 0x01 << MODE_OFFSET,
	HANDICAP_MASK = 0x8F << HANDICAP_OFFSET,
	SYNC_MASK     = 0x03 << SYNC_OFFSET,
	SIDE_MASK     = 0x0F << SIDE_OFFSET,
	
	LOCK_BS_MASK  = 0x01 << LOCK_BS_OFFSET,
	AI_MASK       = 0x01 << AI_OFFSET,
};

#define INTERNAL_MASK (AI_MASK | LOCK_BS_MASK)

#define TO_TEAM_MASK(x) ((x) << TEAM_OFFSET)
#define TO_ALLY_MASK(x) ((x) << ALLY_OFFSET)
#define TO_HANDICAP_MASK(x) ((x) << HANDICAP_OFFSET)
#define TO_SIDE_MASK(x) ((x) << SIDE_OFFSET)

#define SYNCED (1 << SYNC_OFFSET)
#define UNSYNCED (2 << SYNC_OFFSET)
#define TO_SIDE_MASK(x) ((x) << SIDE_OFFSET)

#define FROM_TEAM_MASK(x) (((x) & TEAM_MASK) >> TEAM_OFFSET)
#define FROM_ALLY_MASK(x) (((x) & ALLY_MASK) >> ALLY_OFFSET)
#define FROM_HANDICAP_MASK(x) (((x) & HANDICAP_MASK) >> HANDICAP_OFFSET)
#define FROM_SIDE_MASK(x) (((x) & SIDE_MASK) >> SIDE_OFFSET)

#define INGAME_OFFSET 0
#define AWAY_OFFSET 1
#define RANK_OFFSET 2
#define ACCESS_OFFSET 5
#define BOT_OFFSET 6

enum {
	CS_INGAME_MASK = 0x01 << INGAME_OFFSET,
	CS_AWAY_MASK  = 0x01 << AWAY_OFFSET,
	CS_RANK_MASK  = 0x07 << RANK_OFFSET,
	CS_ACCESS_MASK  = 0x01 << ACCESS_OFFSET,
	CS_BOT_MASK  = 0x01 << BOT_OFFSET,
};

#define TO_INGAME_MASK(x) ((x) << INGAME_OFFSET)
#define TO_AWAY_MASK(x) ((x) << AWAY_OFFSET)
#define TO_RANK_MASK(x) ((x) << RANK_OFFSET)
#define TO_ACCESS_MASK(x) ((x) << ACCESS_OFFSET)
#define TO_BOT_MASK(x) ((x) << BOT_OFFSET)

#define FROM_RANK_MASK(x) (((x) & CS_RANK_MASK) >> RANK_OFFSET)

enum USER_SYNC_STATUS {
	USER_SYNC_UNKNOWN = 0,
	USER_SYNC_SYNCED = 1,
	USER_SYNC_UNSYNCED = 2,
};

#define USER_OR_BOT_HEADER\
	char name[MAX_NAME_LENGTH_NUL];\
	uint32_t battleStatus;\
	struct battle *battle;\
	union {\
		uint32_t color;\
		struct {\
			uint8_t red, green, blue;\
		};\
	}

typedef struct User {
	USER_OR_BOT_HEADER;

	HWND chatWindow;
	uint32_t cpu, id;
	
	char alias[MAX_NAME_LENGTH_NUL];
	char scriptPassword[128];
	uint8_t clientStatus, country, ignore;
}User;

typedef struct Bot {
	USER_OR_BOT_HEADER;
	
	char *dll;
	int nbOptions;
	struct Option2 *options;
	User *owner;
	struct Bot *next;
} Bot;


//Identify them with AI_MASK of battleStatus
typedef union UserOrBot {
	struct {
		USER_OR_BOT_HEADER;
	};
	User user;
	Bot bot;
}UserOrBot;

#undef USER_OR_BOT_HEADER


typedef struct battle {
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

#define START_RECT_MAX 200

typedef enum StartPosType {
	STARTPOS_FIXED = 0,
	STARTPOS_RANDOM = 1,
	STARTPOS_CHOOSE_INGAME = 2,
	STARTPOS_CHOOSE_BEFORE = 3,
}StartPosType;

#define HOST_SPADS 0x02
#define HOST_RELAY 0x01 
#define HOST_LOCAL 0x03
#define HOST_SP    0x05
#define HOST_FLAG  0x01

typedef struct Option {
	char *key, *val, *def;
	char _data[];
}Option;


#define INVALID_STARTPOS (StartPos){-1, -1}
#define GET_STARTPOS(_team) (gBattleOptions.startPosType == STARTPOS_CHOOSE_BEFORE && *(uint64_t *)&gBattleOptions.positions[_team] != -1\
		? gBattleOptions.positions[_team]\
		: gMapInfo.positions[_team])
	
typedef struct BattleOptions {
	uint8_t hostType;
	StartPosType startPosType;
	RECT startRects[NUM_ALLIANCES];
	StartPos positions[NUM_ALLIANCES];
	uint32_t modHash;
}BattleOption;

extern size_t gNbModOptions, gNbMapOptions;
extern Option *gModOptions, *gMapOptions;
extern uint32_t gMapHash, gModHash;
extern MapInfo gMapInfo;

extern char **gMaps, **gMods;
extern size_t gNbMaps, gNbMods;


extern User gMyUser;
extern Battle *gMyBattle;
extern uint32_t battleToJoin;
extern BattleOption gBattleOptions;
char gSideNames[16][128];

extern uint32_t gUdpHelpPort;

Battle * FindBattle(uint32_t id);
User * FindUser(const char username[]);


Battle *NewBattle(void);
void DelBattle(Battle *);
User * NewUser(uint32_t id, const char *name);
User *GetNextUser(void);
void DelUser(User *u);

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

void AddBot(const char *name, User *owner, uint32_t battleStatus, uint32_t color, const char *aiDll);
void DelBot(const char *name);

void ResetData(void);
void ResetBattleOptions(void);
void JoinedBattle(Battle *b, uint32_t modHash);
void UpdateBattleStatus(UserOrBot *s, uint32_t battleStatus, uint32_t color);

#define GetNumPlayers(_b)\
	((_b)->nbParticipants - (_b)->nbSpectators - (_b)->nbBots)
