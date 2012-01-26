#include <assert.h>

#include "wincommon.h"
#include "data.h"
#include "client.h"
#include "settings.h"
#include "sync.h"
#include "countrycodes.h"
#include "client_message.h"
#include "alphalobby.h"
#include "spring.h"


#define LAUNCH_SPRING(path)\
	CreateThread(NULL, 0, _launchSpring2, (LPVOID)(wcsdup(path)), 0, NULL);
static DWORD WINAPI _launchSpring2(LPVOID path)
{
	PROCESS_INFORMATION processInfo = {};
	if (CreateProcess(NULL, path, NULL, NULL, 0, 0, NULL, NULL, &(STARTUPINFO){.cb=sizeof(STARTUPINFO)}, &processInfo)) {
		// SetClientStatus(CS_INGAME_MASK, CS_INGAME_MASK);
		SetBattleStatus(&gMyUser, 0, READY_MASK);
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		SetClientStatus(0, CS_INGAME_MASK);
	} else
		MyMessageBox("Failed to launch spring", "Check that the path is correct in 'Options>Lobby Preferences'.");
	free(path);
	return 0;
}

void LaunchSpring(void)
//Script is unreadably compact because it might need to be sent to relayhost.
//This lets us send it in 1-2 packets, so it doesnt take 5 seconds like in springlobby.
{
	if (!*gMyUser.name)
		strcpy(gMyUser.name, "Player");

	Battle *b = gMyBattle;
	assert(b);

	char buff[8192], *buffEnd=buff;
	
	#define APPEND_LINE(format, ...) \
		(buffEnd += sprintf(buffEnd, (format), ## __VA_ARGS__))
	
	if (gBattleOptions.hostType == HOST_RELAY && gBattleOptions.startPosType != 2)
		gBattleOptions.startPosType = 3;
	int startPosType = gBattleOptions.hostType == HOST_RELAY && gBattleOptions.startPosType != 2 ? 3 : gBattleOptions.startPosType;
	
	if (gBattleOptions.hostType == HOST_SP)
		APPEND_LINE("[GAME]{IsHost=1;MyPlayerName=%s;", gMyUser.name);
	else
		APPEND_LINE("[GAME]{HostIP=%s;HostPort=%hu;IsHost=%hu;MyPlayerName=%s;MyPasswd=%s;",
			b->ip, b->port, gBattleOptions.hostType & HOST_FLAG && 
			!(gBattleOptions.hostType == HOST_RELAY && (gMyBattle->founder->clientStatus & CS_INGAME_MASK))
			, gMyUser.name, gMyUser.scriptPassword);

	if (!(gBattleOptions.hostType & HOST_FLAG) || (gBattleOptions.hostType == HOST_RELAY && (gMyBattle->founder->clientStatus & CS_INGAME_MASK))) {
		*buffEnd++ = '}';
		goto done;
	}
	APPEND_LINE("ModHash=%u;MapHash=%u;MapName=%s;GameType=%s;startpostype=%u;",
			gBattleOptions.modHash, b->mapHash, b->mapName, b->modName, startPosType);

	typedef struct Option {
		char *key, *val;
	}Option;

	if (gMapOptions && gMapOptions[0].key) {
		assert(gMapOptions);
		APPEND_LINE("[MAPOPTIONS]{");
		for (int i=0; i<gNbMapOptions; ++i)
			if (gMapOptions[i].val && strcmp(gMapOptions[i].val, gMapOptions[i].def))
				APPEND_LINE("%s=%s;", gMapOptions[i].key, gMapOptions[i].val);
		*buffEnd++ = '}';
	}
	if (gModOptions && gModOptions[0].key) {
		assert(gModOptions);
		APPEND_LINE("[MODOPTIONS]{");
		for (int i=0; i<gNbModOptions; ++i)
			if (gModOptions[i].val && strcmp(gModOptions[i].val, gModOptions[i].def))
				APPEND_LINE("%s=%s;", gModOptions[i].key, gModOptions[i].val);
		*buffEnd++ = '}';
	}

	// int nbTeams=0;
	
	int allyBitfield = 0;
	int teamBitfield = 0;
	
	#define DOSTARTPOS() {\
		if (startPosType == STARTPOS_CHOOSE_BEFORE) {\
			StartPos pos = GET_STARTPOS(FROM_TEAM_MASK(battleStatus));\
			APPEND_LINE("StartPosX=%d;StartPosZ=%d;", pos.x, pos.z);\
		}\
	}
	//Don't put the relay host in the script
	int relayHostOffset = gBattleOptions.hostType == HOST_RELAY;
	
	for (int i=relayHostOffset; i < b->nbParticipants; ++i) {
		const UserOrBot *s = b->users[i];
		uint32_t battleStatus = s->battleStatus;
		
		
		if (battleStatus & AI_MASK) {
			const Bot *bot = &s->bot;
			int hostId=relayHostOffset;
			while (hostId < b->nbParticipants
					&& b->users[hostId] != (UserOrBot *)bot->owner)
				++hostId;
			assert(b->users[hostId] == (UserOrBot *)bot->owner);
			char *version = strchr(bot->dll, '|');
			APPEND_LINE("[AI%d]{ShortName=%.*s;Team=%d;Host=%d;Version=%s;", i, version - bot->dll, bot->dll, FROM_TEAM_MASK(battleStatus), hostId, version+1);
			
			if (bot->options) {
				APPEND_LINE("[Options]{");
				for (int i=0; i<bot->nbOptions; ++i)
					if (bot->options[i].val)
						APPEND_LINE("%s=%s;", bot->options[i].key, bot->options[i].val);
				*buffEnd++ = '}';
			}
			if (!(teamBitfield & 1 << FROM_TEAM_MASK(battleStatus))) {
				teamBitfield &= ~(1 << FROM_TEAM_MASK(battleStatus));
				APPEND_LINE("}[TEAM%d]{TeamLeader=%d;AllyTeam=%d;RGBColor=%g %g %g;Side=%s;", FROM_TEAM_MASK(battleStatus), hostId, FROM_ALLY_MASK(battleStatus), bot->red/255.f, bot->green/255.f, bot->blue/255.f, gSideNames[FROM_SIDE_MASK(battleStatus)]);
				DOSTARTPOS()
			}
		} else {
			const User *u = &s->user;
			APPEND_LINE("[PLAYER%d]{Name=%s;CountryCode=%.2s;Rank=%d;Password=%s;Team=%d;",
					i, u->name, countryCodes[u->country], FROM_RANK_MASK(u->clientStatus), u->scriptPassword, FROM_TEAM_MASK(battleStatus));

			if (!(u->battleStatus & MODE_MASK))
				APPEND_LINE("Spectator=1;");
			else if (!(teamBitfield & 1 << FROM_TEAM_MASK(battleStatus))) {
				teamBitfield &= ~(1 << FROM_TEAM_MASK(battleStatus));
				APPEND_LINE("}[TEAM%d]{TeamLeader=%d;AllyTeam=%d;RGBColor=%g %g %g;Side=%s;", FROM_TEAM_MASK(battleStatus), i, FROM_ALLY_MASK(battleStatus),
					u->red/255.f, u->green/255.f, u->blue/255.f, gSideNames[FROM_SIDE_MASK(battleStatus)]);
				DOSTARTPOS()
			}
				
		}
		*buffEnd++ = '}';
		if (battleStatus & MODE_MASK)
			allyBitfield |= 1 << FROM_ALLY_MASK(battleStatus);
	}

	for (int i=0; i<16; ++i) {
		if (!(allyBitfield & 1 << i))
			continue;
		
		APPEND_LINE("[ALLYTEAM%d]{", i);
		if (startPosType == STARTPOS_CHOOSE_INGAME &&
				(gBattleOptions.startRects[i].left | gBattleOptions.startRects[i].top | gBattleOptions.startRects[i].right | gBattleOptions.startRects[i].bottom))
			APPEND_LINE("StartRectLeft=%g;StartRectTop=%g;StartRectRight=%g;StartRectBottom=%g;",
				gBattleOptions.startRects[i].left/200.f, gBattleOptions.startRects[i].top/200.f,
				gBattleOptions.startRects[i].right/200.f, gBattleOptions.startRects[i].bottom/200.f);
		*buffEnd++ = '}';
	}
	
	*buffEnd++ = '}';
	*buffEnd++ = '\0';
	
	if (gBattleOptions.hostType == HOST_RELAY) {
		SendToServer("!CLEANSCRIPT");
	
		//Server won't accept messages longer than 1024, so we need to break it up
		for (char *s=buff; *s;) {
			size_t len = min(buffEnd - s, 900);
			len += strcspn(s+len, ";}{"); //must not split an identifer
			SendToServer("!APPENDSCRIPTLINE %.*s", len, s);
			s += len;
		}
		SendToServer("!STARTGAME");
		return;
	}
	done:;
	
	FILE *fp = fopen("script_.txt", "w");
	if (!fp) {
		MessageBox(NULL, L"Could not open script.txt for writing", L"Launch failed", MB_OK);
		fclose(fp);
		return;
	}
	fwrite(buff, 1, buffEnd - buff, fp);
	fclose(fp);
	wchar_t path[128];
	swprintf(path, L"%hs script_.txt", gSettings.spring_path);
	LAUNCH_SPRING(path);
	return;
}

void LaunchReplay(const wchar_t *replayName)
{
	wchar_t path[MAX_PATH];
	swprintf(path, L"%hs demos/%s", gSettings.spring_path, replayName);
	printf("path = \"%ls\"\n", path);
	LAUNCH_SPRING(path);
}
