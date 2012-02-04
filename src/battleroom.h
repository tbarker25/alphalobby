#pragma once

#include "common.h"

#define WC_BATTLEROOM L"BattleRoom"

#define WM_SETMODDETAILS (WM_APP+202)
#define WM_CHANGEMOD     (WM_APP+204)
#define WM_RESYNC        (WM_APP+206)

struct User;
union UserOrBot;
typedef struct HWND__ * HWND;

extern HWND gBattleRoomWindow;

#define GetBattleChat()\
	GetDlgItem(gBattleRoomWindow, DEST_BATTLE)

void BattleRoom_UpdateUser(union UserOrBot *);
void BattleRoom_RemoveUser(const union UserOrBot *u);

void BattleRoom_Show(void);
void BattleRoom_Hide(void);
bool BattleRoom_IsAutoUnspec(void);
void BattleRoom_ChangeMinimapBitmap(const uint16_t *mapPixels,
		uint16_t metalMapWidth, uint16_t metalMapHeight, const uint8_t *metalMapPixels,
		uint16_t heightMapWidth, uint16_t heightMapHeight, const uint8_t *heightMapPixels);
void BattleRoom_StartPositionsChanged(void);
