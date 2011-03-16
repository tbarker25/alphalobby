#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wincommon.h"
#include <winsock2.h>

#include "alphalobby.h"
#include "battlelist.h"
#include "battleroom.h"
#include "battletools.h"
#include "chat.h"
#include "client_message.h"
#include "countrycodes.h"
#include "data.h"
#include "dialogboxes.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "userlist.h"

#include "client.h"

#undef NDEBUG

static FILE *agreement;
extern uint32_t gLastBattleStatus;
extern uint8_t gLastClientStatus;
const char **gRelayManagers;
int gRelayManagersCount;

static char *command;

#define RECV_SIZE 8192
#define MAX_MESSAGE_LENGTH 1024 //Hardcoded into server

void copyNextWord(char *s) {
	size_t len = strcspn(command, " ");
	char *word = command;
	command += len + !!command[len];
	word[len] = '\0';
	memcpy(s, word, len + 1);
}

char * getNextWord(void) {
	size_t len = strcspn(command, " ");
	char *word = command;
	command += len + !!command[len];
	word[len] = '\0';
	return word;
}

int getNextInt(void) {
	return atoi(getNextWord());
}

void copyNextSentence(char *s) {
	size_t len = strcspn(command, "\t");
	char *word = command;
	command += len + !!command[len];
	word[len] = '\0';
	memcpy(s, word, len + 1);
}

//Declare server message functions:
#define X(name, func)\
	static void name(void);
#include "messages.def"
#undef X

//Define server message functions:
#define X(name, func)\
	static void name(void) {func;}
#include "messages.def"
#undef X

//Initialize array of server message functions:
static const struct {
	void (*func)(void);
	char *name;
}serverCommands[] = {
#define X(name, func)\
	{name, #name},
#include "messages.def"
#undef X
};

static SOCKET sock = INVALID_SOCKET;
DWORD lastPing;

void PollServer(void)
{
	//Static since incomplete commands overflow for next call:
	static size_t bufferLength = 0;
	static char buffer[RECV_SIZE+MAX_MESSAGE_LENGTH];
	
	int bytesReceived = 0;
	if ((bytesReceived = recv(sock, buffer + bufferLength, RECV_SIZE, 0)) <= 0) {
		printf("bytes recv = %d, err = %d\n", bytesReceived, WSAGetLastError());
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
		HWND serverWindow = GetServerChat();
		if (GetTabIndex(serverWindow) >= 0)
			Chat_Said(serverWindow, NULL, CHAT_SERVERIN, s);
		
		command = s;
		char *commandName = getNextWord();
		size_t commandLength = command - commandName;

		for (int i=0; i<lengthof(serverCommands); ++i) {
			if (!memcmp(commandName, serverCommands[i].name, commandLength)) {
				serverCommands[i].func();
				break;
			}
		}
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
	int commandStart=0;
	if (format[0] == '!') {
		if (*relayHoster)
			commandStart = sprintf(buff, "SAYPRIVATE %s ", relayHoster);
		else
			++format;
	}
	size_t len = vsprintf(buff + commandStart, format, args) + commandStart;
	va_end(args);
	if (commandStart)
		for (char *s = &buff[commandStart]; *s && *s != ' '; ++s)
			*s |= 0x20; //tolower
	#ifndef NDEBUG
	printf("< %s\n", buff);
	#endif
	HWND serverWindow = GetServerChat();
	if (GetTabIndex(serverWindow) >= 0)
		Chat_Said(serverWindow, NULL, CHAT_SERVEROUT, buff);
	buff[len] = '\n';

	if (send(sock, buff, len+1, 0) == SOCKET_ERROR) {
		assert(0);
		Disconnect();
		MyMessageBox("Connection to server interupted", "Please check your internet connection.");
	}
}

void Disconnect(void)
{
	KillTimer(gMainWindow, 1);
	shutdown(sock, SD_BOTH);
	closesocket(sock);
	sock = INVALID_SOCKET;
	WSACleanup();
	MainWindow_ChangeConnect(0);
	ResetData();
	BattleList_Reset();
}

void CALLBACK Ping(HWND window, UINT msg, UINT_PTR idEvent, DWORD dwTime)
{
	lastPing = GetTickCount();
	SendToServer("PING");
}


DWORD WINAPI _Connect(void (*onFinish)(void)) 
{
	if (sock != INVALID_SOCKET)
		Disconnect();
	
	SetStatus(L"\t\tconnecting");
	if (WSAStartup(MAKEWORD(2,2), &(WSADATA){})) {
		MyMessageBox("Could not connect to server.", "WSAStartup failed.\nPlease check your internet connection.");
		return 1;
	}
	
	struct hostent *host = gethostbyname("taspringmaster.clan-sy.com");
	if (!host) {
		MyMessageBox("Could not connect to server.", "Could not retrieve host information.\nPlease check your internet connection.");
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
		MyMessageBox("Could not connect to server.", "Could not finalize connection.\nPlease check your internet connection.");
		closesocket(sock);
		sock = INVALID_SOCKET;
	}

	WSAAsyncSelect(sock, gMainWindow, WM_POLL_SERVER, FD_READ|FD_CLOSE);
	onFinish();
	
	SetTimer(gMainWindow, 1, 30000 / 2, Ping);
	return 0;
}

#if 0
char * (void)
//TODO: real error handling
{
	char hostName[256];
	WSASetLastError(0);
	if (gethostname(hostName, sizeof(hostName))) {
		printf("last error = %d\n", WSAGetLastError());
		assert(0);
		return NULL;
	}
	
	struct hostent *host = NULL;
	if (!(host = gethostbyname(hostName))) {
		printf("last error = %d\n", WSAGetLastError());
		assert(0);
		return NULL;
	}

	char *ret;
	if (!(ret = inet_ntoa(*(struct in_addr *)host->h_addr))) {
		printf("last error = %d\n", WSAGetLastError());
		assert(0);
	}
	return ret;
}
#endif

void Connect(void (*onFinish)(void))
{
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE )_Connect, (LPVOID)onFinish, 0, 0);
}
