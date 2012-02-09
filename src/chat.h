#pragma once
#include "common.h"
#define WC_CHATBOX L"ChatBox"

struct User;

extern HWND _gServerChatWindow;
extern HWND gChatWindow;

void Chat_Init(void);
void Chat_Said(HWND window, const char *username, ChatType type, const char *text);
HWND GetChannelChat(const char *name);
HWND GetPrivateChat(struct User *);
#define GetServerChat()\
	(_gServerChatWindow)

void UpdateUser(struct User *u);
void ChatWindow_AddUser(HWND window, struct User *u);
void ChatWindow_RemoveUser(HWND window, struct User *u);
void SaveLastChatWindows(void);

