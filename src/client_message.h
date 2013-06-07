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

void SendToServer(const char *format, ...);

void JoinBattle(uint32_t id, const char *password);

union UserOrBot;
void SetBattleStatusAndColor(union UserOrBot *s, uint32_t orMask, uint32_t nandMask, uint32_t color);
#define SetBattleStatus(_s, _orMask, _nandMask)\
	SetBattleStatusAndColor((union UserOrBot *)(_s), (_orMask), (_nandMask), -1)
#define SetColor(_s, color)\
	SetBattleStatusAndColor((union UserOrBot *)(_s), 0, 0, (color))
void Kick(union UserOrBot *);

void SetClientStatus(uint8_t s, uint8_t mask);
void RequestChannels(void);
void LeaveBattle(void);
void JoinChannel(const char *chanName, int focus);
void LeaveChannel(const char *chanName);
void RenameAccount(const char *newUsername);
void ChangePassword(const char *oldPassword, const char *newPassword);
void RegisterAccount(const char *username, const char *password);
char Autologin(void);
void Login(const char *username, const char *password);
void OpenBattle(const char *title, const char *password, const char *modName, const char *mapName, uint16_t port);
void OpenRelayBattle(const char *title, const char *password, const char *modName, const char *mapName, const char *manager);
void ConfirmAgreement(void);
void RequestIngameTime(const char *username);
void ChangeMap(const char *mapName);

extern int lastStatusUpdate, gLastAutoMessage;

#endif /* end of include guard: CLIENT_MESSAGE_H */
