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

#ifndef BATTLELIST_H
#define BATTLELIST_H

#define WC_BATTLELIST L"BattleListClass"

struct Battle;

void BattleList_add_battle(struct Battle *);
void BattleList_update_battle(const struct Battle *);
void BattleList_close_battle(const struct Battle *);
void BattleList_on_end_login_info(void);
void BattleList_reset(void);

#endif /* end of include guard: BATTLELIST_H */
