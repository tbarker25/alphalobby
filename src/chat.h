#pragma once
#include "common.h"
#define WC_CHATBOX L"ChatBox"

struct User;

void Chat_Said(HWND window, const char *username, ChatType type, const char *text);
HWND GetChannelChat(const char *name);
HWND GetPrivateChat(struct User *);
HWND GetServerChat(void);

// #define GetServerChat()
	// GetDlgItem(gMainWindow, 0x2000)

void UpdateUser(struct User *u);
void ChatWindow_AddUser(HWND window, struct User *u);
void ChatWindow_RemoveUser(HWND window, struct User *u);
void SaveLastChatWindows(void);

