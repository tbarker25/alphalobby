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

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <Commctrl.h>

#include "alphalobby.h"
#include "battleroom.h"
#include "chat.h"
#include "chat_window.h"
#include "client.h"
#include "client_message.h"
#include "data.h"
#include "dialogboxes.h"
#include "host_default.h"
#include "md5.h"
#include "settings.h"
#include "sync.h"

int lastStatusUpdate, gLastAutoMessage;
static char myPassword[BASE16_MD5_LENGTH];
static char myUserName[MAX_NAME_LENGTH+1];
uint32_t gLastBattleStatus;
uint8_t gLastClientStatus;

void JoinBattle(uint32_t id, const char *password)
{
	static const char *passwordToJoin;
	Battle *b = FindBattle(id);

	if (!b) {
		assert(0);
		return;
	}

	if (b == gMyBattle) {
		BattleRoom_Show();
		return;
	}

	ChangedMod(b->modName);
	if (gMapHash != b->mapHash || !gMapHash)
		ChangedMap(b->mapName);
	
	if (gMyBattle) {
		battleToJoin = id;
		free((void *)passwordToJoin);
		passwordToJoin = strdup(password);
		LeaveBattle();

	} else {
		battleToJoin = 0;
		gMyUser.battleStatus = 0;
		password = password ?: passwordToJoin;
		//NB: This section must _not_ be accessed from PollServer
		if (b->passworded && !password) {
			char *buff = alloca(1024);
			buff[0] = '\0';
			if (GetTextDlg("Enter password", buff, 1024))
				return;
			password = buff;
		}
		gHostType = &gHostDefault;
		BattleRoom_Show();
		SendToServer("JOINBATTLE %u %s %x",
				id, password ?: "",
				(uint32_t)(rand() ^ (rand() << 16)));
		free((void *)passwordToJoin);
	}
}


void SetBattleStatusAndColor(union UserOrBot *s, uint32_t orMask, uint32_t nandMask, uint32_t color)
{
	if (!s)
		return;
	if (color == -1)
		color = s->color;
	uint32_t bs = ((orMask & nandMask) | (s->battleStatus & ~nandMask)) & ~INTERNAL_MASK;
	
	if (&s->user == &gMyUser) {
		uint32_t bs = (orMask & nandMask) | (gLastBattleStatus & ~nandMask & ~SYNC_MASK) | GetSyncStatus() | READY_MASK;
		if (bs != gLastBattleStatus || color != gMyUser.color) {
			gLastBattleStatus=bs;
			if (battleInfoFinished)
				SendToServer("MYBATTLESTATUS %d %d", bs & ~INTERNAL_MASK, color);
		}
		return;
	}
	
	if ((s->battleStatus & ~INTERNAL_MASK) == bs && color == s->color)
		return;
	
	if (s->battleStatus & AI_MASK) {
		SendToServer("UPDATEBOT %s %d %d" + (s->bot.owner == &gMyUser), s->name, bs, color);
		return;
	}

	if (nandMask & TEAM_MASK)
		if (gHostType->forceTeam)
			gHostType->forceTeam(s->name, FROM_TEAM_MASK(orMask));
	if (nandMask & ALLY_MASK)
		if (gHostType->forceAlly)
			gHostType->forceAlly(s->name, FROM_ALLY_MASK(orMask));

	if (gHostType->forceTeam)
		return;

	assert(0);
}

void Kick(union UserOrBot *s)
{
	if (s->battleStatus & AI_MASK) {
		SendToServer("REMOVEBOT %s", s->name);
	} else if (gHostType->kick) {
		gHostType->kick(s->name);
	} else {
		assert(0);
	}
}

void SetClientStatus(uint8_t s, uint8_t mask)
{
	uint8_t cs = (s & mask) | (gLastClientStatus & ~mask);
	if (cs != gLastClientStatus)
		SendToServer("MYSTATUS %d", (gLastClientStatus=cs));
}

void LeaveChannel(const char *chanName)
{
	SendDlgItemMessage(GetChannelChat(chanName), 2, LVM_DELETEALLITEMS, 0, 0);
	SendToServer("LEAVE %s", chanName);
}

void RequestIngameTime(const char username[])
{
	SendToServer("GETINGAMETIME %s", username?:"");
}


void LeaveBattle(void)
{
	SendToServer("LEAVEBATTLE");
}

void ChangeMap(const char *mapName)
{
	if (gHostType->setMap) {
		gHostType->setMap(mapName);
	} else {
		SendToServer("UPDATEBATTLEINFO 0 0 %d %s",
				GetMapHash(mapName), mapName);
	}
}

void RequestChannels(void)
{
	SendToServer("CHANNELS");
}

void JoinChannel(const char *chanName, int focus)
{
	chanName += *chanName == '#';
	
	for (const char *c=chanName; *c; ++c) {
		if (!isalnum(*c) && *c != '[' && *c != ']') {
			MyMessageBox("Couldn't join channel", "Unicode channels are not allowed.");
			return;
		}
	}
		
	SendToServer("JOIN %s", chanName);
	if (focus)
		ChatWindow_SetActiveTab(GetChannelChat(chanName));
}

void RenameAccount(const char *newUsername)
{
	SendToServer("RENAMEACCOUNT %s", newUsername);
}

void ChangePassword(const char *oldPassword, const char *newPassword)
{
	SendToServer("CHANGEPASSWORD %s %s", oldPassword, newPassword);
}

void login(void)
{
/* LOGIN userName password cpu localIP {lobby name and version} [userID] [{compFlags}] */
	SendToServer("LOGIN %s %s 0 * AlphaLobby"
	#ifdef VERSION
	" " STRINGIFY(VERSION)
	#endif
	"\t0\ta m sp", myUserName, myPassword);//, GetLocalIP() ?: "*");
}

static void registerAccount(void)
{
	SendToServer("REGISTER %s %s", myUserName, myPassword);
}

void RegisterAccount(const char *username, const char *password)
{
	strcpy(myUserName, username);
	strcpy(myPassword, password);
	Connect(registerAccount);
}

char Autologin(void)
{
	const char *s;
	s = LoadSetting("username");
	if (!s)
		return 0;
	strcpy(myUserName, s);
	s = LoadSetting("password");
	if (!s)
		return 0;
	strcpy(myPassword, s);
	Connect(login);
	return 1;
}

void Login(const char *username, const char *password)
// LOGIN username password cpu localIP {lobby name and version} [{userID}] [{compFlags}]
{
	strcpy(myUserName, username);
	strcpy(myPassword, password);
	Connect(login);
}

void ConfirmAgreement(void)
{
	SendToServer("CONFIRMAGREEMENT");
	login();
}

void OpenBattle(const char *title, const char *password, const char *modName, const char *mapName, uint16_t port)
{
	SendToServer("OPENBATTLE 0 0 %s %hu 16 %d 0 %d %s\t%s\t%s", password ?: "*", port, GetModHash(modName), GetMapHash(mapName), mapName, title, modName);
}
