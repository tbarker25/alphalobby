#pragma once

typedef enum SplitType {
	SPLIT_HORZ,
	SPLIT_VERT,
	SPLIT_CORNERS1,
	SPLIT_CORNERS2,
	SPLIT_RAND,
	
	SPLIT_LAST = SPLIT_RAND,
}SplitType;

union UserOrBot;

void SetSplit(SplitType, int size);
void SetMap(const char *name);
void FixPlayerStatus(const union UserOrBot *u);
//use NULL to fix all players in battle

void Rebalance(void);
//Calls FixPlayerStatus(NULL)

uint32_t GetNewBattleStatus(void);
