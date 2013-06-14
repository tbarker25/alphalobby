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

#define GetBattleChat()\
	GetDlgItem(g_battle_room, DEST_BATTLE)

struct Option;
union UserOrBot;
typedef struct HWND__* HWND;

void BattleRoom_hide(void);
bool BattleRoom_is_auto_unspec(void);
void BattleRoom_on_change_mod(void);
void BattleRoom_on_left_battle(const union UserOrBot *);
void BattleRoom_on_set_mod_details(void);
void BattleRoom_on_set_option(struct Option *opt);
void BattleRoom_resize_columns(void);
void BattleRoom_show(void);
void BattleRoom_update_user(union UserOrBot *);

extern HWND g_battle_room;


#endif /* end of include guard: BATTLEROOM_H */
