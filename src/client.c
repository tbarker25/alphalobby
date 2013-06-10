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
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>
#include <winsock2.h>

#include "battle.h"
#include "battlelist.h"
#include "chat.h"
#include "client.h"
#include "client_message.h"
#include "common.h"
#include "host_relay.h"
#include "mainwindow.h"
#include "messages.h"
#include "mybattle.h"
#include "user.h"

#define RECV_SIZE 8192
#define MAX_MESSAGE_LENGTH 1024 //Hardcoded into server

static SOCKET sock = INVALID_SOCKET;

void
Server_poll(void)
{
	//Static since incomplete commands overflow for next call:
	static size_t buf_len = 0;
	static char buf[RECV_SIZE+MAX_MESSAGE_LENGTH];

	int bytes_received = recv(sock, buf + buf_len, RECV_SIZE, 0);
	if (bytes_received <= 0) {
		printf("bytes recv = %d, err = %d\n", bytes_received,
				WSAGetLastError());
		Server_disconnect();
		assert(bytes_received == 0);
		return;
	}

	buf_len += bytes_received;

	char *s = buf;
	for (size_t i=0; i<buf_len; ++i) {
		if (buf[i] != '\n')
			continue;
		buf[i] = '\0';
		#ifndef NDEBUG
		printf("> %s\n", s);
		#endif
		/* HWND server_window = Chat_get_server_window(); */
		/* Chat_said(server_window, NULL, CHAT_SERVERIN, s); */
		Messages_handle(s);

		s = buf + i + 1;
	}
	buf_len -= s - buf;

	memmove(buf, s, buf_len);
}

void
Server_send(const char *format, ...)
{
	if (sock == INVALID_SOCKET)
		return;

	char buf[MAX_MESSAGE_LENGTH]; //NOTE this is coded at server level...

	va_list args;
	va_start (args, format);
	int command_start = 0;

	size_t len = vsprintf(buf + command_start, format, args) + command_start;
	va_end(args);

	if (command_start)  //Use lowercase for relay host
		for (char *s = &buf[command_start]; *s && *s != ' '; ++s)
			*s |= 0x20; //tolower
	#ifndef NDEBUG
	printf("< %s\n", buf);
	#endif

	/* HWND server_window = Chat_get_server_window(); */
	/* if (GetTabIndex(server_window) >= 0) */
		/* Chat_said(server_window, NULL, CHAT_SERVEROUT, buf); */
	buf[len] = '\n';

	if (send(sock, buf, len+1, 0) == SOCKET_ERROR) {
		assert(0);
		Server_disconnect();
		MainWindow_msg_box("Connection to server interupted",
				"Please check your internet connection.");
	}
}

void
Server_disconnect(void)
{
	KillTimer(g_main_window, 1);
	shutdown(sock, SD_BOTH);
	closesocket(sock);
	sock = INVALID_SOCKET;
	WSACleanup();
	MainWindow_change_connect(CONNECTION_OFFLINE);

	if (g_my_battle)
		MyBattle_left_battle();
	BattleList_reset();
	Battles_reset();
	Users_reset();
	Chat_on_disconnect();
}

void CALLBACK
Server_ping(__attribute__((unused)) HWND window, __attribute__((unused)) UINT msg,
		__attribute__((unused)) UINT_PTR idEvent, __attribute__((unused)) DWORD dwTime)
{
	Server_send("PING");
}

static DWORD WINAPI
connect_proc(void (*on_finish)(void))
{
	if (sock != INVALID_SOCKET)
		Server_disconnect();
	MainWindow_change_connect(CONNECTION_CONNECTING);

	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2,2), &wsa_data)) {
		MainWindow_msg_box("Could not connect to server.",
				"WSAStartup failed.\nPlease check your internet connection.");
		return 1;
	}

	struct hostent *host = gethostbyname("lobby.springrts.com");
	if (!host) {
		MainWindow_msg_box("Could not connect to server.",
				"Could not retrieve host information.\nPlease check your internet connection.");
		return 1;
	}
	#define HTONS(x) (uint16_t)((x) << 8 | (x) >> 8)
	struct sockaddr_in server = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = *((unsigned long*)host->h_addr),
		.sin_port = HTONS((uint16_t)8200),
	};
	sock = socket(PF_INET, SOCK_STREAM, 0);

	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
		MainWindow_msg_box("Could not connect to server.",
				"Could not finalize connection.\nPlease check your internet connection.");
		closesocket(sock);
		sock = INVALID_SOCKET;
	}

	WSAAsyncSelect(sock, g_main_window, WM_POLL_SERVER, FD_READ|FD_CLOSE);
	on_finish();

	SetTimer(g_main_window, 1, 30000 / 2, Server_ping);
	return 0;
}

enum ServerStatus Server_status(void)
{
	return sock != INVALID_SOCKET;
}

void
Server_connect(void (*on_finish)(void))
{
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE )connect_proc, (LPVOID)on_finish, 0, 0);
}
