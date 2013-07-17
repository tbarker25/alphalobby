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

#ifndef CHAT_TAB_H
#define CHAT_TAB_H

#define WC_CHATTAB L"ChatTab"

struct User;
enum ChatType;

void ChatTab_add_user_to_channel        (const char *channel, const struct User *);
void ChatTab_focus_channel   (const char *channel);
void ChatTab_focus_private   (struct User *);
void ChatTab_on_said_private (struct User *, const char *text, enum ChatType);
void ChatTab_on_said_channel (const char *channel, struct User *, const char *text, enum ChatType);
void ChatTab_remove_user_from_channel(const char *channel, const struct User *);
void ChatTab_join_channel(const char *channel_name);

#endif /* end of include guard: CHAT_TAB_H */
