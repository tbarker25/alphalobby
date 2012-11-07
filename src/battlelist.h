#pragma once

#define WC_BATTLELIST L"BattleListClass"

struct battle;
void BattleList_UpdateBattle(struct battle *);
void BattleList_CloseBattle(struct battle *);
void BattleList_OnEndLoginInfo(void);
void BattleList_Reset(void);

extern HWND gBattleList;
