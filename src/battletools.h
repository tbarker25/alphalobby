#pragma once

typedef enum SplitType {
	// SPLIT_FIXED,
	// SPLIT_RANDOM,
	
	SPLIT_FIRST = 8,
	SPLIT_HORIZONTAL = SPLIT_FIRST,
	SPLIT_VERTICAL,
	SPLIT_CORNERS1,
	SPLIT_CORNERS2,
	SPLIT_LAST = SPLIT_CORNERS2,
}SplitType;

union UserOrBot;

void SetSplit(SplitType, int size);
void SetMap(const char *name);
void FixPlayerStatus(const union UserOrBot *u);
//use NULL to fix all players in battle

void Rebalance(void);
//Calls FixPlayerStatus(NULL)

uint32_t GetNewBattleStatus(void);