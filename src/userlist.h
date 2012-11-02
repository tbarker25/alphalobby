#pragma once

struct User;

void UserList_Show(void);
void UserList_AddUser(struct User *);
void UserList_RemoveUser(struct User *);

void ChannelList_Show(void);
void ChannelList_AddChannel(const char *channame, const char *usercount, const char *description);

