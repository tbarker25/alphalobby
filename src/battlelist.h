#pragma once

#define WC_BATTLELIST L"BattleListClass"

struct Battle;
void BattleList_UpdateBattle(struct Battle *);
void BattleList_CloseBattle(struct Battle *);
void BattleList_OnEndLoginInfo(void);
void BattleList_Reset(void);

extern HWND gBattleList;
