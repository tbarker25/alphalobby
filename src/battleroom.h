#pragma once

#define WC_BATTLEROOM L"BattleRoom"

struct User;
struct Option;
union UserOrBot;
typedef struct HWND__* HWND;

extern HWND gBattleRoom;

#define GetBattleChat()\
	GetDlgItem(gBattleRoom, DEST_BATTLE)

void BattleRoom_UpdateUser(union UserOrBot *);
void BattleRoom_RemoveUser(const union UserOrBot *u);

void BattleRoom_Show(void);
void BattleRoom_Hide(void);
char BattleRoom_IsAutoUnspec(void);
void BattleRoom_ChangeMinimapBitmap(const uint16_t *mapPixels,
		uint16_t metalMapWidth, uint16_t metalMapHeight, const uint8_t *metalMapPixels,
		uint16_t heightMapWidth, uint16_t heightMapHeight, const uint8_t *heightMapPixels);
void BattleRoom_StartPositionsChanged(void);
void BattleRoom_VoteStarted(const char *topic);
#define BattleRoom_VoteEnded() (BattleRoom_VoteStarted(NULL))
void BattleRoom_RedrawMinimap(void);
void BattleRoom_OnChangeMod(void);
void BattleRoom_OnSetModDetails(void);
void BattleRoom_OnSetOption(struct Option *opt);
