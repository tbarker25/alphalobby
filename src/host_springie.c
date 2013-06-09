#include <inttypes.h>
#include <stdio.h>
#include <windows.h>

#include "battle.h"
#include "battleroom.h"
#include "chat.h"
#include "client_message.h"
#include "common.h"
#include "host_spads.h"
#include "mybattle.h"
#include "user.h"

static void saidBattle(const char *userName, char *text);

const HostType gHostSpringie = {
	.saidBattle = saidBattle,
};

static void saidBattle(const char *userName, char *text)
{
	if (!strcmp(userName, gMyBattle->founder->name)
			&& text[0] == '[') {

		int braces = 1;
		for (char *s = text + 1; *s; ++s){
			braces += *s == '[';
			braces -= *s == ']';
			if (braces == 0){
				*s = '\0';
				Chat_Said(GetBattleChat(), text + 1, CHAT_INGAME, s + 1);
				return;
			}
		}
	}
	Chat_Said(GetBattleChat(), userName, 0, text);
}
