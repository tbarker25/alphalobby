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

#ifndef CHAT_WINDOW_H
#define CHAT_WINDOW_H

#define WC_CHATWINDOW L"ChatWindow"

extern HWND gChatWindow;

void ChatWindow_SetActiveTab(HWND tab);
int ChatWindow_AddTab(HWND tab);
void ChatWindow_RemoveTab(HWND tab);

#endif /* end of include guard: CHAT_WINDOW_H */
