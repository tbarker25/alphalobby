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

#include "alphalobby.h"
#include "battlelist.h"
#include "chat.h"
#include "client.h"
#include "client_message.h"
#include "data.h"
#include "messages.h"
#include "host_relay.h"

#define RECV_SIZE 8192
#define MAX_MESSAGE_LENGTH 1024 //Hardcoded into server

static SOCKET sock = INVALID_SOCKET;

void PollServer(void)
{
	//Static since incomplete commands overflow for next call:
	static size_t bufferLength = 0;
	static char buffer[RECV_SIZE+MAX_MESSAGE_LENGTH];
	
	int bytesReceived = recv(sock, buffer + bufferLength, RECV_SIZE, 0);
	if (bytesReceived <= 0) {
		printf("bytes recv = %d, err = %d\n", bytesReceived,
				WSAGetLastError());
		Disconnect();
		assert(bytesReceived == 0);
		return;
	}

	bufferLength += bytesReceived;
	
	char *s = buffer;
	for (int i=0; i<bufferLength; ++i) {
		if (buffer[i] != '\n')
			continue;
		buffer[i] = '\0';
		#ifndef NDEBUG
		printf("> %s\n", s);
		#endif
		/* HWND serverWindow = GetServerChat(); */
		/* Chat_Said(serverWindow, NULL, CHAT_SERVERIN, s); */
		handleCommand(s);
		
		s = buffer + i + 1;
	}
	bufferLength -= s - buffer;
	
	memmove(buffer, s, bufferLength);
}

void SendToServer(const char *format, ...)
{
	if (sock == INVALID_SOCKET)
		return;

	char buff[MAX_MESSAGE_LENGTH]; //NOTE this is coded at server level...
	
	va_list args;
	va_start (args, format);
	int commandStart = 0;

	size_t len = vsprintf(buff + commandStart, format, args) + commandStart;
	va_end(args);
	
	if (commandStart)  //Use lowercase for relay host
		for (char *s = &buff[commandStart]; *s && *s != ' '; ++s)
			*s |= 0x20; //tolower
	#ifndef NDEBUG
	printf("< %s\n", buff);
	#endif

	/* HWND serverWindow = GetServerChat(); */
	/* if (GetTabIndex(serverWindow) >= 0) */
		/* Chat_Said(serverWindow, NULL, CHAT_SERVEROUT, buff); */
	buff[len] = '\n';

	if (send(sock, buff, len+1, 0) == SOCKET_ERROR) {
		assert(0);
		Disconnect();
		MyMessageBox("Connection to server interupted",
				"Please check your internet connection.");
	}
}

void Disconnect(void)
{
	KillTimer(gMainWindow, 1);
	shutdown(sock, SD_BOTH);
	closesocket(sock);
	sock = INVALID_SOCKET;
	WSACleanup();
	MainWindow_ChangeConnect(CONNECTION_OFFLINE);

	if (gMyBattle)
		LeftBattle();
	BattleList_Reset();
	ResetBattles();
	ResetUsers();
	Chat_OnDisconnect();
}

void CALLBACK Ping(HWND window, UINT msg, UINT_PTR idEvent, DWORD dwTime)
{
	SendToServer("PING");
}

DWORD WINAPI _Connect(void (*onFinish)(void)) 
{
	if (sock != INVALID_SOCKET)
		Disconnect();
	MainWindow_ChangeConnect(CONNECTION_CONNECTING);
	
	if (WSAStartup(MAKEWORD(2,2), &(WSADATA){})) {
		MyMessageBox("Could not connect to server.",
				"WSAStartup failed.\nPlease check your internet connection.");
		return 1;
	}
	
	struct hostent *host = gethostbyname("lobby.springrts.com");
	if (!host) {
		MyMessageBox("Could not connect to server.",
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
		MyMessageBox("Could not connect to server.",
				"Could not finalize connection.\nPlease check your internet connection.");
		closesocket(sock);
		sock = INVALID_SOCKET;
	}

	WSAAsyncSelect(sock, gMainWindow, WM_POLL_SERVER, FD_READ|FD_CLOSE);
	onFinish();
	
	SetTimer(gMainWindow, 1, 30000 / 2, Ping);
	return 0;
}

enum ConnectionState GetConnectionState(void)
{
	return sock != INVALID_SOCKET;
}

void Connect(void (*onFinish)(void))
{
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE )_Connect, (LPVOID)onFinish, 0, 0);
}
