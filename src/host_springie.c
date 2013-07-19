#include <inttypes.h>
#include <stdio.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "chatbox.h"
#include "common.h"
#include "host_spads.h"
#include "host_springie.h"
#include "mybattle.h"
#include "user.h"

static void said_battle(User *, char *text, ChatType);

void
Springie_set_as_host(void)
{
	MyBattle_said_battle = said_battle;
}

static void
said_battle(User *user, char *text, ChatType chat_type)
{
	if (user == g_my_battle->founder
	    && chat_type == CHAT_NORMAL
	    && text[0] == '[') {
		int braces;

		braces = 1;
		for (char *s = text + 1; *s; ++s) {
			braces += *s == '[';
			braces -= *s == ']';
			if (braces == 0) {
				*s = '\0';
				BattleRoom_said_battle(text + 1, s + 1,
				    CHAT_INGAME);
				return;
			}
		}
	}

	BattleRoom_said_battle(user->name, text, chat_type);
}
