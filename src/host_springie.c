#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>

#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "chat.h"
#include "common.h"
#include "host_spads.h"
#include "mybattle.h"
#include "user.h"

static void said_battle(const char *username, char *text);

const HostType HOST_SPRINGIE = {
	.said_battle = said_battle,
};

static void
said_battle(const char *username, char *text)
{
	if (!strcmp(username, g_my_battle->founder->name)
			&& text[0] == '[') {

		int braces = 1;
		for (char *s = text + 1; *s; ++s){
			braces += *s == '[';
			braces -= *s == ']';
			if (braces == 0){
				*s = '\0';
				Chat_said(GetBattleChat(), text + 1, CHAT_INGAME, s + 1);
				return;
			}
		}
	}
	Chat_said(GetBattleChat(), username, 0, text);
}
