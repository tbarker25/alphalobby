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

#ifndef CLIENT_MESSAGE_H
#define CLIENT_MESSAGE_H


void JoinBattle(uint32_t id, const char *password);

union UserOrBot;
void SetBattleStatusAndColor(union UserOrBot *s, uint32_t or_mask, uint32_t nand_mask, uint32_t color);
#define SetBattleStatus(_s, _or_mask, _nand_mask)\
	SetBattleStatusAndColor((union UserOrBot *)(_s), (_or_mask), (_nand_mask), -1)
#define SetColor(_s, color)\
	SetBattleStatusAndColor((union UserOrBot *)(_s), 0, 0, (color))
void Kick(union UserOrBot *);

void SetClientStatus(uint8_t s, uint8_t mask);
void RequestChannels(void);
void LeaveBattle(void);
void JoinChannel(const char *channel_name, int focus);
void LeaveChannel(const char *channel_name);
void RenameAccount(const char *new_username);
void Change_password(const char *old_password, const char *new_password);
void RegisterAccount(const char *username, const char *password);
char Autologin(void);
void Login(const char *username, const char *password);
void OpenBattle(const char *title, const char *password, const char *mod_name, const char *map_name, uint16_t port);
void RelayHost_open_battle(const char *title, const char *password, const char *mod_name, const char *map_name, const char *manager);
void ConfirmAgreement(void);
void RequestIngame_time(const char *username);
void ChangeMap(const char *map_name);

extern int      g_last_auto_message;
extern int      g_last_status_update;
extern uint32_t g_last_battle_status;
extern uint8_t  g_last_client_status;

#endif /* end of include guard: CLIENT_MESSAGE_H */
