/*
 * Copyright (c) 2013, Thomas Barker
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * It under the terms of the GNU General Public License as published by
 * The Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * Along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BATTLEROOM_H
#define BATTLEROOM_H

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

#endif /* end of include guard: BATTLEROOM_H */
