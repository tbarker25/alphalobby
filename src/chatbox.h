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

#ifndef CHATBOX_H
#define CHATBOX_H

#define WC_CHATBOX L"ChatBox"

struct User;

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

typedef cdecl void (SayFunction)(const char *text, bool is_ex, const void *dst);

void ChatBox_append           (HWND window, const char *username, ChatType type, const char *text);
void ChatBox_set_say_function (HWND window, SayFunction, const char *dest);

#endif /* end of include guard: CHATBOX_H */
