#pragma once
#define WC_CHATBOX L"ChatBox"

struct User;

extern HWND gChatWindow;

typedef enum ChatType {
	CHAT_NORMAL,
	CHAT_SELF,
	CHAT_SYSTEM,
	CHAT_SELFEX,
	CHAT_EX,
	CHAT_INGAME,
	CHAT_TOPIC,
	CHAT_SERVERIN,
	CHAT_SERVEROUT,
	CHAT_LAST = CHAT_SERVEROUT,
}ChatType;

void Chat_Said(HWND window, const char *username, ChatType type, const char *text);
HWND GetChannelChat(const char *name);
HWND GetPrivateChat(struct User *);
HWND GetServerChat(void);

// void UpdateUser(struct User *u);
void ChatWindow_AddUser(HWND window, struct User *u);
void ChatWindow_RemoveUser(HWND window, struct User *u);
void SaveLastChatWindows(void);

