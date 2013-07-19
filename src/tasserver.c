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

#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>

#include "battle.h"
#include "battlelist.h"
#include "battleroom.h"
#include "chatbox.h"
#include "chattab.h"
#include "tasserver.h"
#include "common.h"
#include "host_relay.h"
#include "mainwindow.h"
#include "md5.h"
#include "messages.h"
#include "mybattle.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "user.h"
#include "dialogs/gettextdialog.h"

#define RECV_SIZE 8192
#define MAX_MESSAGE_LENGTH 1024 //Hardcoded into server
#define HTONS(x) (uint16_t)((x) << 8 | (x) >> 8)

static uint32_t WINAPI connect_proc(void (*on_finish)(void));
static void login(void);
static void register_account(void);
static void send_to_server(const char *format, ...) __attribute__ ((format (ms_printf, 1, 2)));
static void CALLBACK ping(HWND, uint32_t, uintptr_t, uint32_t);

uint32_t g_last_auto_message;
BattleStatus g_last_battle_status;
ClientStatus g_last_client_status;
static SOCKET s_socket = INVALID_SOCKET;
static char s_my_password[BASE16_MD5_LENGTH];
static char s_my_username[MAX_NAME_LENGTH+1];

void
TasServer_poll(void)
{
	/* static since incomplete commands overflow for next call */
	static size_t buf_len;
	static char buf[RECV_SIZE+MAX_MESSAGE_LENGTH];

	int bytes_received;
	char *s;

	bytes_received = recv(s_socket, buf + buf_len, RECV_SIZE, 0);
	if (bytes_received <= 0) {
		printf("bytes recv = %d, err = %d\n",
		    bytes_received, WSAGetLastError());
		TasServer_disconnect();
		assert(bytes_received == 0);
		return;
	}

	buf_len += (size_t)bytes_received;

	s = buf;
	for (size_t i=0; i<buf_len; ++i) {
		if (buf[i] != '\n')
			continue;
		buf[i] = '\0';

		#ifndef NDEBUG
		printf("> %s\n", s);
		#endif

		Messages_handle(s);

		s = buf + i + 1;
	}
	buf_len -= (size_t)(s - buf);

	memmove(buf, s, buf_len);
}

static void
send_to_server(const char *format, ...)
{
	char buf[MAX_MESSAGE_LENGTH];
	va_list args;
	int len;

	if (s_socket == INVALID_SOCKET)
		return;

	va_start (args, format);
	len = vsprintf(buf, format, args);
	assert((size_t)len < sizeof buf);
	va_end(args);

	#ifndef NDEBUG
	printf("< %s\n", buf);
	#endif

	buf[len++] = '\n';

	if (send(s_socket, buf, len, 0) == SOCKET_ERROR) {
		assert(0);
		TasServer_disconnect();
		MainWindow_msg_box("Connection to server interupted",
		    "Please check your internet connection.");
	}
}

void
TasServer_disconnect(void)
{
	KillTimer(g_main_window, 1);
	shutdown(s_socket, SD_BOTH);
	closesocket(s_socket);
	s_socket = INVALID_SOCKET;
	WSACleanup();
	MainWindow_change_connect(CONNECTION_OFFLINE);

	if (g_my_battle)
		MyBattle_left_battle();

	BattleList_reset();
	Battles_reset();
	Users_reset();
	/* Chat_on_disconnect(); */
}

static void CALLBACK
ping(__attribute__((unused)) HWND window, __attribute__((unused)) uint32_t msg,
		__attribute__((unused)) uintptr_t idEvent, __attribute__((unused)) uint32_t dwTime)
{
	send_to_server("PING");
}

static uint32_t WINAPI
connect_proc(void (*on_finish)(void))
{
	WSADATA wsa_data;
	struct hostent *host;
	struct sockaddr_in server;

	if (s_socket != INVALID_SOCKET)
		TasServer_disconnect();
	MainWindow_change_connect(CONNECTION_CONNECTING);

	if (WSAStartup(MAKEWORD(2,2), &wsa_data)) {
		MainWindow_msg_box("Could not connect to server.",
		    "WSAStartup failed.\n"
		    "Please check your internet connection.");
		return 1;
	}

	host = gethostbyname("lobby.springrts.com");
	if (!host) {
		MainWindow_msg_box("Could not connect to server.",
		    "Could not retrieve host information.\n"
		    "Please check your internet connection.");
		return 1;
	}

	server = (struct sockaddr_in){
		.sin_family = AF_INET,
		.sin_addr.s_addr = *((unsigned long*)host->h_addr),
		.sin_port = HTONS((uint16_t)8200),
	};

	s_socket = socket(PF_INET, SOCK_STREAM, 0);

	if (connect(s_socket, (struct sockaddr *)&server, sizeof server) == SOCKET_ERROR) {
		MainWindow_msg_box("Could not connect to server.",
		    "Could not finalize connection.\n"
		    "Please check your internet connection.");
		closesocket(s_socket);
		s_socket = INVALID_SOCKET;
	}

	WSAAsyncSelect(s_socket, g_main_window, WM_POLL_SERVER,
	    FD_READ|FD_CLOSE);
	on_finish();

	SetTimer(g_main_window, 1, 30000 / 2, (void *)ping);
	return 0;
}

enum ServerStatus
TasServer_status(void)
{
	return s_socket != INVALID_SOCKET;
}

void
TasServer_connect(void (*on_finish)(void))
{
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)connect_proc,
	    (void *)on_finish, 0, 0);
}

void
TasServer_send_join_battle(uint32_t id, const char *password)
{
	static const char *password_to_join;
	Battle *b;

	b = Battles_find(id);
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
		TasServer_send_leave_battle();

	} else {
		g_battle_to_join = 0;
		g_my_user.BattleStatus = (BattleStatus){0};
		password = password ?: password_to_join;
		//NB: This section must _not_ be accessed from TasServer_poll
		if (b->passworded && !password) {
			char *buf = _alloca(1024);
			buf[0] = '\0';
			if (GetTextDialog_create("Enter password", buf, 1024))
				return;
			password = buf;
		}
		MyBattle_reset();
		BattleRoom_show();

		send_to_server("JOINBATTLE %u %s %x",
		    id, password ?: "",
		    (uint32_t)(rand() ^ (rand() << 16)));

		free((void *)password_to_join);
	}
}

void
TasServer_send_my_battle_status(struct BattleStatus battle_status)
{
	union {
		BattleStatus; uint32_t as_int;
	} bs, last_bs;

	bs.BattleStatus = battle_status;

	bs.sync = Sync_is_synced() ? SYNC_SYNCED : SYNC_UNSYNCED;
	last_bs.BattleStatus = g_last_battle_status;

	if (bs.as_int != last_bs.as_int) {
		g_last_battle_status = battle_status;
		send_to_server("MYBATTLESTATUS %d %d",
		    bs.as_int, g_my_user.color);
	}
}

void
TasServer_send_my_color(uint32_t color)
{
	union {
		BattleStatus; uint32_t as_int;
	} bs;

	bs.BattleStatus = g_my_user.BattleStatus;
	send_to_server("MYBATTLESTATUS %d %d",
	    bs.as_int, color);
}

void
TasServer_send_my_client_status(ClientStatus status)
{
	union {
		ClientStatus; uint8_t as_int;
	} cs, last_cs;

	cs.ClientStatus = status;
	cs.ingame = Spring_is_ingame();
	last_cs.ClientStatus = g_last_client_status;

	if (cs.as_int != last_cs.as_int) {
		g_last_client_status = cs.ClientStatus;
		send_to_server("MYSTATUS %d", cs.as_int);

	}
}

void
TasServer_send_leave_channel(const char *channel_name)
{
	/* SendDlgItemMessage(Chat_get_channel_window(channel_name), 2, LVM_DELETEALLITEMS, 0, 0); */
	send_to_server("LEAVE %s", channel_name);
}

void
TasServer_send_get_ingame_time(void)
{
	send_to_server("GETINGAMETIME");
}

void
TasServer_send_change_map(const char *map_name)
{
	MyBattle_set_map(map_name);
}

void
TasServer_send_rename(const char *new_username)
{
	send_to_server("RENAMEACCOUNT %s", new_username);
}

void
TasServer_send_ring(const User *user)
{
	send_to_server("RING %s", user->name);
}

void
TasServer_send_change_password(const char *old_password, const char *new_password)
{
	send_to_server("CHANGEPASSWORD %s %s", old_password, new_password);
}

static void
login(void)
{
/* LOGIN username password cpu local_i_p {lobby name and version} [user_id] [{comp_flags}] */
	send_to_server("LOGIN %s %s 0 * AlphaLobby"
	#ifdef VERSION
	" " STRINGIFY(VERSION)
	#endif
	"\t0\ta m sp", s_my_username, s_my_password);//, GetLocalIP() ?: "*");
}

static void
register_account(void)
{
	send_to_server("REGISTER %s %s", s_my_username, s_my_password);
}

void
TasServer_send_register(const char *username, const char *password)
{
	strcpy(s_my_username, username);
	strcpy(s_my_password, password);
	TasServer_connect(register_account);
}

void
TasServer_send_login(const char *username, const char *password)
/* LOGIN username password cpu local_i_p {lobby name and version} [{user_id}] [{comp_flags}] */
{
	if (username && password) {
		strcpy(s_my_username, username);
		strcpy(s_my_password, password);
	}

	assert(*s_my_username && *s_my_password);

	if (TasServer_status()) {
		login();

	} else {
		TasServer_connect(login);
	}
}

void
TasServer_send_confirm_agreement(void)
{
	send_to_server("CONFIRMAGREEMENT");
	login();
}

void
TasServer_send_get_channels(void)
{
	send_to_server("Channels");
}

void
TasServer_send_join_channel(const char *channel_name)
{
	send_to_server("JOIN %s", channel_name);
}

void
TasServer_send_open_battle(const char *password, uint16_t port, const char *mod_name, const char *map_name, const char *description)
{
	send_to_server("OPENBATTLE 0 0 %s %hu 16 %d 0 %d %s\t%s\t%s",
	    password ?: "*", port, Sync_mod_hash(mod_name),
	    Sync_map_hash(map_name), map_name, description, mod_name);
}

void
TasServer_send_leave_battle(void)
{
	assert(g_my_battle);
	send_to_server("LEAVEBATTLE");
}

void
TasServer_send_say_battle(const char *text, bool is_ex)
{
	send_to_server(is_ex ? "SAYBATTLEEX %s" : "SAYBATTLE %s", text);
}

void
TasServer_send_say_channel(const char *text, bool is_ex, const char *dst)
{
	send_to_server(is_ex ? "SAYEX %s %s" : "SAY %s %s", dst, text);
}

void
TasServer_send_say_private(const char *text, bool is_ex, const char *username)
{
	send_to_server(is_ex ? "SAYPRIVATEEX %s %s" : "SAYPRIVATE %s %s",
	    username, text);
}

void
TasServer_send_force_ally(const char *name, int ally)
{
	send_to_server("FORCEALLYNO %s %d", name, ally);
}

void
TasServer_send_force_team(const char *name, int team)
{
	send_to_server("FORCETEAMNO %s %d" , name, team);
}

void
TasServer_send_force_spectator(const User *user)
{
	send_to_server("FORCESPECTATORMODE %s", user->name);
}

void
TasServer_send_kick(const UserBot *u)
{
	if (u->ai) {
		send_to_server("REMOVEBOT %s", u->name);

	} else {
		send_to_server("KICKFROMBATTLE %s", u->name);
	}
}

void
TasServer_send_set_map(const char *map_name)
{
	send_to_server("UPDATEBATTLEINFO 0 0 %d %s",
			Sync_map_hash(map_name), map_name);
}

void
TasServer_send_add_start_box(int i, int left, int top, int right, int bottom)
{
	send_to_server("ADDSTARTRECT %d %d %d %d %d",
	    i, left, top, right, bottom);
}

void
TasServer_send_del_start_box(int i)
{
	send_to_server("REMOVESTARTRECT %d", i);
}

void
TasServer_send_script_tags(const char *script_tags)
{
	send_to_server("SETSCRIPTTAGS %s", script_tags);
}

void
TasServer_send_force_color(const User *user, uint32_t color)
{
	send_to_server("FORCETEAMCOLOR %s %d", user->name, color);
}
