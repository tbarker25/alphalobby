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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <Commctrl.h>

#include "battle.h"
#include "battleroom.h"
#include "chat.h"
#include "chat_window.h"
#include "client.h"
#include "client_message.h"
#include "common.h"
#include "dialogboxes.h"
#include "mainwindow.h"
#include "md5.h"
#include "mybattle.h"
#include "settings.h"
#include "sync.h"
#include "user.h"

int      g_last_auto_message;
int      g_last_status_update;
uint32_t g_last_battle_status;
uint8_t  g_last_client_status;

static char my_password[BASE16_MD5_LENGTH];
static char my_username[MAX_NAME_LENGTH+1];

void
JoinBattle(uint32_t id, const char *password)
{
	static const char *password_to_join;
	Battle *b = Battles_find(id);

	if (!b) {
		assert(0);
		return;
	}

	if (b == g_my_battle) {
		BattleRoom_show();
		return;
	}

	Sync_on_changed_mod(b->mod_name);
	if (g_map_hash != b->map_hash || !g_map_hash)
		Sync_on_changed_map(b->map_name);

	if (g_my_battle) {
		g_battle_to_join = id;
		free((void *)password_to_join);
		password_to_join = _strdup(password);
		LeaveBattle();

	} else {
		g_battle_to_join = 0;
		g_my_user.battle_status = 0;
		password = password ?: password_to_join;
		//NB: This section must _not_ be accessed from Server_poll
		if (b->passworded && !password) {
			char *buf = _alloca(1024);
			buf[0] = '\0';
			if (GetTextDlg("Enter password", buf, 1024))
				return;
			password = buf;
		}
		g_host_type = NULL;
		BattleRoom_show();
		Server_send("JOINBATTLE %u %s %x",
				id, password ?: "",
				(uint32_t)(rand() ^ (rand() << 16)));
		free((void *)password_to_join);
	}
}


void
SetBattleStatusAndColor(union UserOrBot *s, uint32_t or_mask,
		uint32_t nand_mask, uint32_t color)
{
	if (!s)
		return;
	if (color == (uint32_t)-1)
		color = s->color;
	uint32_t bs = ((or_mask & nand_mask) | (s->battle_status & ~nand_mask)) & ~BS_INTERNAL;

	if (&s->user == &g_my_user) {
		uint32_t bs = (or_mask & nand_mask) | (g_last_battle_status & ~nand_mask & ~BS_SYNC) | Sync_get_status() | BS_READY;
		if (bs != g_last_battle_status || color != g_my_user.color) {
			g_last_battle_status=bs;
			if (g_battle_info_finished)
				Server_send("MYBATTLESTATUS %d %d", bs & ~BS_INTERNAL, color);
		}
		return;
	}

	if ((s->battle_status & ~BS_INTERNAL) == bs && color == s->color)
		return;

	if (s->battle_status & BS_AI) {
		Server_send("UPDATEBOT %s %d %d" + (s->bot.owner == &g_my_user), s->name, bs, color);
		return;
	}

	if (nand_mask & BS_TEAM)
		if (g_host_type && g_host_type->force_team)
			g_host_type->force_team(s->name, FROM_BS_TEAM(or_mask));

	if (nand_mask & BS_ALLY)
		if (g_host_type && g_host_type->force_ally)
			g_host_type->force_ally(s->name, FROM_BS_ALLY(or_mask));
}

void
Kick(union UserOrBot *s)
{
	if (s->battle_status & BS_AI) {
		Server_send("REMOVEBOT %s", s->name);
		return;
	}

	if (g_host_type && g_host_type->kick) {
		g_host_type->kick(s->name);
		return;
	}
}

void
SetClientStatus(uint8_t s, uint8_t mask)
{
	uint8_t cs = (s & mask) | (g_last_client_status & ~mask);
	if (cs != g_last_client_status)
		Server_send("MYSTATUS %d", (g_last_client_status=cs));
}

void
LeaveChannel(const char *channel_name)
{
	SendDlgItemMessage(Chat_get_channel_window(channel_name), 2, LVM_DELETEALLITEMS, 0, 0);
	Server_send("LEAVE %s", channel_name);
}

void
RequestIngame_time(const char username[])
{
	Server_send("GETINGAMETIME %s", username?:"");
}


void
LeaveBattle(void)
{
	Server_send("LEAVEBATTLE");
}

void
ChangeMap(const char *map_name)
{
	if (g_host_type && g_host_type->set_map) {
		g_host_type->set_map(map_name);
	}
}

void
RequestChannels(void)
{
	Server_send("CHANNELS");
}

void
JoinChannel(const char *channel_name, int focus)
{
	channel_name += *channel_name == '#';

	for (const char *c=channel_name; *c; ++c) {
		if (!isalnum(*c) && *c != '[' && *c != ']') {
			MainWindow_msg_box("Couldn't join channel", "Unicode channels are not allowed.");
			return;
		}
	}

	Server_send("JOIN %s", channel_name);
	if (focus)
		ChatWindow_set_active_tab(Chat_get_channel_window(channel_name));
}

void
RenameAccount(const char *new_username)
{
	Server_send("RENAMEACCOUNT %s", new_username);
}

void
Change_password(const char *old_password, const char *new_password)
{
	Server_send("CHANGEPASSWORD %s %s", old_password, new_password);
}

void
login(void)
{
/* LOGIN username password cpu local_i_p {lobby name and version} [user_id] [{comp_flags}] */
	Server_send("LOGIN %s %s 0 * AlphaLobby"
	#ifdef VERSION
	" " STRINGIFY(VERSION)
	#endif
	"\t0\ta m sp", my_username, my_password);//, GetLocalIP() ?: "*");
}

static void
register_account(void)
{
	Server_send("REGISTER %s %s", my_username, my_password);
}

void
RegisterAccount(const char *username, const char *password)
{
	strcpy(my_username, username);
	strcpy(my_password, password);
	Server_connect(register_account);
}

char Autologin(void)
{
	const char *s;
	s = Settings_load_str("username");
	if (!s)
		return 0;
	strcpy(my_username, s);
	s = Settings_load_str("password");
	if (!s)
		return 0;
	strcpy(my_password, s);
	Server_connect(login);
	return 1;
}

void
Login(const char *username, const char *password)
// LOGIN username password cpu local_i_p {lobby name and version} [{user_id}] [{comp_flags}]
{
	strcpy(my_username, username);
	strcpy(my_password, password);
	Server_connect(login);
}

void
ConfirmAgreement(void)
{
	Server_send("CONFIRMAGREEMENT");
	login();
}

void
OpenBattle(const char *title, const char *password, const char *mod_name, const char *map_name, uint16_t port)
{
	Server_send("OPENBATTLE 0 0 %s %hu 16 %d 0 %d %s\t%s\t%s", password ?: "*", port, Sync_mod_hash(mod_name), Sync_map_hash(map_name), map_name, title, mod_name);
}
