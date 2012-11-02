#pragma once

typedef void * HWND;

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

typedef struct UserBot {
	char name[MAX_NAME_LENGTH_NUL];
	uint32_t battleStatus;
	struct battle *battle;
	union {
		uint32_t color;
		struct {
			uint8_t red, green, blue;
		};
	};
} UserBot;

typedef struct User {
	UserBot;

	HWND chatWindow;
	uint32_t cpu, id;
	
	char alias[MAX_NAME_LENGTH_NUL];
	char scriptPassword[128];
	uint8_t clientStatus, country, ignore;
}User;

typedef struct Bot {
	UserBot;
	
	char *dll;
	int nbOptions;
	struct Option2 *options;
	User *owner;
	struct Bot *next;
} Bot;

//Identify them with AI_MASK of battleStatus
typedef union UserOrBot {
	UserBot;
	User user;
	Bot bot;
}UserOrBot;