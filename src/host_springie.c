#include <inttypes.h>
#include <stdio.h>
#include <windows.h>

#include "battleroom.h"
#include "battletools.h"
#include "chat.h"
#include "client_message.h"
#include "data.h"
#include "host_spads.h"

static void saidBattle(const char *userName, char *text);
static void saidBattleEx(const char *userName, char *text);
static void vote(int voteYes);

const HostType gHostSpringie = {
	.saidBattle = saidBattle,
	.saidBattleEx = saidBattleEx,
	.vote = vote,
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

static void saidBattleEx(const char *userName, char *text)
{
	// Check for callvote:
	// "Do you want to apply options " + wordFormat + "? !vote 1 = yes, !vote 2 = no"
	// "Do you want to remove current boss " + ah.BossName + "? !vote 1 = yes, !vote 2 = no"
	if (!memcmp("Do you want to ", text, sizeof("Do you want to ") - 1)){
		char *commandStart = text + sizeof("Do you want to ") - 1;
		char *commandEnd = strchr(commandStart, '?');
		if (commandEnd){
			commandStart[0] = toupper(commandStart[0]);
			commandEnd[1] = '\0';
			BattleRoom_VoteStarted(commandStart);
			commandStart[0] = tolower(commandStart[0]);
			commandEnd[1] = ' ';
		}
	}
	// "Springie option 1 has 1 of 1 votes"

	// Check for endvote:
	if (!memcmp("vote successful", text, sizeof("vote successful") - 1)
			|| !memcmp("not enough votes", text, sizeof("not enough votes") - 1)
			|| !memcmp(" poll cancelled", text, sizeof(" poll cancelled") - 1))
		BattleRoom_VoteEnded();
	Chat_Said(GetBattleChat(), userName, CHAT_EX, text);
}

static void vote(int voteYes)
{
	SendToServer("SAYBATTLE !vote %c", '2' - (char)voteYes);
}

