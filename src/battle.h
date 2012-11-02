#pragma once

enum battleType {
	BATTLE_NORMAL = 0,
	BATTLE_REPLAY = 1,
};

enum natType {
	NAT_NONE  = 0,
	NAT_OTHER,
};

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
#define HOST_SPRINGIE 0x10

typedef enum OptionType{
  opt_error = 0, opt_bool = 1, opt_list = 2, opt_number = 3,
  opt_string = 4, opt_section = 5
} OptionType;

typedef struct OptionListItem {
	char *key, *name;
}OptionListItem;

typedef struct Option {
	OptionType type;
	char *key, *name, *val, *def;
	struct Option *section;
	
	size_t nbListItems;
	OptionListItem *listItems;
}Option;

#define INVALID_STARTPOS (StartPos){-1, -1}
#define GET_STARTPOS(_team) (gBattleOptions.startPosType == STARTPOS_CHOOSE_BEFORE && *(uint64_t *)&gBattleOptions.positions[_team] != -1\
		? gBattleOptions.positions[_team]\
		: gMapInfo.positions[_team])
	
typedef struct StartRect {
	uint16_t left, top, right, bottom;
}StartRect;

typedef struct BattleOptions {
	uint8_t hostType;
	StartPosType startPosType;
	StartRect startRects[NUM_ALLIANCES];
	StartPos positions[NUM_ALLIANCES];
	uint32_t modHash;
}BattleOption;

extern size_t gNbModOptions, gNbMapOptions;
extern Option *gModOptions, *gMapOptions;
extern uint32_t gMapHash, gModHash;

struct _LargeMapInfo {
	MapInfo mapInfo;
	char description[256];
	char author[201];
};

extern struct _LargeMapInfo _gLargeMapInfo;
#define gMapInfo (_gLargeMapInfo.mapInfo)

extern char **gMaps, **gMods;
extern ssize_t gNbMaps, gNbMods;


extern User gMyUser;
extern Battle *gMyBattle;
extern uint32_t battleToJoin;
extern BattleOption gBattleOptions;

extern uint8_t gNbSides;
extern char gSideNames[16][32];


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

#define GetAliasOf(name) ((name)[0] == '[' ? strchr((name), ']') + 1 : (name))
