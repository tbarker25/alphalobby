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

#ifndef CHAT_H
#define CHAT_H

#define WC_CHATBOX L"ChatBox"

struct User;

extern HWND g_chat_window;

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

typedef enum ChatDest {
	DEST_BATTLE,
	DEST_PRIVATE,
	DEST_CHANNEL,
	DEST_SERVER,
	DEST_LAST = DEST_SERVER,
}ChatDest;

void Chat_add_user(HWND window, struct User *u);
HWND Chat_get_channel_window(const char *name);
HWND Chat_get_private_window(struct User *);
HWND Chat_get_server_window(void);
void Chat_on_disconnect(void);
void Chat_on_left_battle(HWND window, struct User *u);
void Chat_said(HWND window, const char *username, ChatType type, const char *text);
void Chat_save_windows(void);
void Chat_update_user(struct User *u);


#endif /* end of include guard: CHAT_H */
