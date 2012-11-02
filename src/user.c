#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <windows.h>
#include <windowsx.h>
#include <oleacc.h>
#include <Commctrl.h>
#include "wincommon.h"


#include "data.h"
#include "sync.h"
#include "md5.h"
#include "settings.h"
#include "battlelist.h"
#include "common.h"
#include "client_message.h"
#include "battletools.h"
#include "battleroom.h"

#define ALLOC_STEP 10

User gMyUser;
static User **users;
static size_t nbUsers;

User * FindUser(const char *name)
{
	if (!strcmp(name, gMyUser.name))
		return &gMyUser;
	for (int i=0; i<nbUsers; ++i)
		if (!strcmp(users[i]->name, name))
			return &(*users[i]);
	return NULL;
}

User *GetNextUser(void)
{
	static int last;
	return last < nbUsers ? users[last++] : (last = 0, NULL);
}

User * NewUser(uint32_t id, const char *name)
{

	if (!strcmp(gMyUser.name, name)) {
		gMyUser.id = id;
		return &gMyUser;
	}
	int i=0;
	for (; i<nbUsers; ++i)
		if (users[i]->id == id)
			break;

	if (i == nbUsers) {
		if (nbUsers % ALLOC_STEP == 0)
			users = realloc(users, (nbUsers + ALLOC_STEP) * sizeof(User *));
		++nbUsers;
		users[i] = calloc(1, sizeof(User));
		users[i]->id = id;
		strcpy(users[i]->alias, GetAliasOf(name));
	}
	SetWindowTextA(users[i]->chatWindow, name);
	return users[i];
}

void DelUser(User *u)
{
	u->name[0] = 0;
}
