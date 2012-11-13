#pragma once

#define WC_CHATWINDOW L"ChatWindow"

extern HWND gChatWindow;

void ChatWindow_SetActiveTab(HWND tab);
int ChatWindow_AddTab(HWND tab);
void ChatWindow_RemoveTab(HWND tab);
