#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

#include <windows.h>
#include <Shlwapi.h>

#include "alphalobby.h"
#include "battlelist.h"
#include "battleroom.h"
#include "battletools.h"
#include "chat.h"
#include "chat_window.h"
#include "client.h"
#include "client_message.h"
#include "countrycodes.h"
#include "data.h"
#include "dialogboxes.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "userlist.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

static void accepted(void);
static void addBot(void);
static void addUser(void);
static void addStartRect(void);
static void agreement(void);
static void agreementEnd(void);
static void battleOpened(void);
static void battleClosed(void);
static void broadcast(void);
static void channel(void);
static void channelTopic(void);
static void clientBattleStatus(void);
static void clients(void);
static void clientStatus(void);
static void denied(void);
static void forceQuitBattle(void);
static void join(void);
static void joinBattle(void);
static void joinBattleFailed(void);
static void joined(void);
static void joinedBattle(void);
static void joinFailed(void);
static void left(void);
static void leftBattle(void);
static void loginInfoEnd(void);
static void messageOfTheDay(void);
static void openBattle(void);
static void registrationAccepted(void);
static void removeBot(void);
static void removeStartRect(void);
static void removeUser(void);
static void requestBattleStatus(void);
static void ring(void);
static void said(void);
static void saidBattle(void);
static void saidBattleEx(void);
static void saidEx(void);
static void saidPrivate(void);
static void sayPrivate(void);
static void serverMsg(void);
static void serverMsgBox(void);
static void setScriptTags(void);
static void TASServer(void);
static void updateBattleInfo(void);
static void updateBot(void);


static FILE *agreementFile;
static DWORD timeJoinedBattle;
extern uint32_t gLastBattleStatus;
extern uint8_t gLastClientStatus;
const char **gRelayManagers;
int gRelayManagersCount;
static char *command;

static const struct {
	char name[20];
	void (*func)(void);
} serverCommands[] = {
	{"ACCEPTED", accepted},
	{"ADDBOT", addBot},
	{"ADDSTARTRECT", addStartRect},
	{"ADDUSER", addUser},
	{"AGREEMENT", agreement},
	{"AGREEMENTEND", agreementEnd},
	{"BATTLECLOSED", battleClosed},
	{"BATTLEOPENED", battleOpened},
	{"BROADCAST", broadcast},
	{"CHANNEL", channel},
	{"CHANNELTOPIC", channelTopic},
	{"CLIENTBATTLESTATUS", clientBattleStatus},
	{"CLIENTS", clients},
	{"CLIENTSTATUS", clientStatus},
	{"DENIED", denied},
	{"FORCEQUITBATTLE", forceQuitBattle},
	{"JOIN", join},
	{"JOINBATTLE", joinBattle},
	{"JOINBATTLEFAILED", joinBattleFailed},
	{"JOINED", joined},
	{"JOINEDBATTLE", joinedBattle},
	{"JOINFAILED", joinFailed},
	{"LEFT", left},
	{"LEFTBATTLE", leftBattle},
	{"LOGININFOEND", loginInfoEnd},
	{"MOTD", messageOfTheDay},
	{"OPENBATTLE", openBattle},
	{"REGISTRATIONACCEPTED", registrationAccepted},
	{"REMOVEBOT", removeBot},
	{"REMOVESTARTRECT", removeStartRect},
	{"REMOVEUSER", removeUser},
	{"REQUESTBATTLESTATUS", requestBattleStatus},
	{"RING", ring},
	{"SAID", said},
	{"SAIDBATTLE", saidBattle},
	{"SAIDBATTLEEX", saidBattleEx},
	{"SAIDEX", saidEx},
	{"SAIDPRIVATE", saidPrivate},
	{"SAYPRIVATE", sayPrivate},
	{"SERVERMSG", serverMsg},
	{"SERVERMSGBOX", serverMsgBox },
	{"SETSCRIPTTAGS", setScriptTags},
	{"TASServer", TASServer},
	{"UPDATEBATTLEINFO", updateBattleInfo},
	{"UPDATEBOT", updateBot},
};

static void copyNextWord(char *s) {
	size_t len = strcspn(command, " ");
	char *word = command;
	command += len + !!command[len];
	word[len] = '\0';
	memcpy(s, word, len + 1);
}

static char * getNextWord(void) {
	size_t len = strcspn(command, " ");
	char *word = command;
	command += len + !!command[len];
	word[len] = '\0';
	return word;
}

static int getNextInt(void) {
	return atoi(getNextWord());
}

static void copyNextSentence(char *s) {
	size_t len = strcspn(command, "\t");
	char *word = command;
	command += len + !!command[len];
	word[len] = '\0';
	memcpy(s, word, len + 1);
}

void handleCommand(char *s)
{

	command = s;
	char *commandName = getNextWord();
	typeof(*serverCommands) *com =
		bsearch(commandName, serverCommands, LENGTH(serverCommands),
				sizeof(*serverCommands), (void *)strcmp);
	if (com)
		com->func();
}

static void accepted(void)
{
	copyNextWord(gMyUser.name);
	strcpy(gMyUser.alias, GetAliasOf(gMyUser.name));
}


static void addBot(void)
{
	// ADDBOT BATTLE_ID name owner battlestatus teamcolor{AIDLL}
	__attribute__((unused))
	char *battleID = getNextWord();
	assert(atoi(battleID) == gMyBattle->id);
	char *name = getNextWord();
	User *owner = FindUser(getNextWord());
	assert(owner);
	uint32_t battleStatus = getNextInt();
	uint32_t color = getNextInt();
	AddBot(name, owner, battleStatus, color, command);
}

static void addUser(void)
{
	char *name = getNextWord();
	uint8_t country = 0;
	for (int i=0; i < LENGTH(countryCodes); ++i)
		if (*((uint16_t *)command) == *((uint16_t *)countryCodes[i]))
			country = i;
	command += 3;
	uint32_t cpu = getNextInt();

	User *u = NewUser(getNextInt(), name);
	strcpy(u->name, name);
	u->country = country;
	u->cpu = cpu;

	ChatWindow_AddUser(GetServerChat(), u);
	if (gSettings.flags & (1<<DEST_SERVER))
		Chat_Said(GetServerChat(), u->name, CHAT_SYSTEM, "has logged in");

	if (!strcmp(u->name, relayHoster)){
		if (*relayCmd){
			SendToServer(relayCmd);
			*relayCmd = '\0';
		}
		return;
	}
}

static void addStartRect(void)
{
	typeof(*gBattleOptions.startRects) *rect = &gBattleOptions.startRects[getNextInt()];
	rect->left = getNextInt();
	rect->top = getNextInt();
	rect->right = getNextInt();
	rect->bottom = getNextInt();
	BattleRoom_StartPositionsChanged();
}

static void agreement(void)
{
	agreementFile = agreementFile ?: tmpfile();
	fputs(command, agreementFile);
}

static void agreementEnd(void)
{
	rewind(agreementFile);
	SendMessage(gMainWindow, WM_EXECFUNCPARAM, (WPARAM)CreateAgreementDlg, (LPARAM)agreementFile);
	agreementFile = NULL;
}

static void battleOpened(void)
{
	Battle *b = NewBattle();

	b->id = getNextInt();
	b->type = getNextInt();
	b->natType = getNextInt();

	char founderName[MAX_NAME_LENGTH_NUL];
	copyNextWord(founderName);
	b->founder = FindUser(founderName);
	assert(b->founder);
	b->nbParticipants = 1;
	b->founder->battle = b;

	copyNextWord(b->ip);
	b->port = getNextInt();
	b->maxPlayers = getNextInt();
	b->passworded = getNextInt();
	b->rank = getNextInt();
	b->mapHash = getNextInt();
	copyNextSentence(b->mapName);
	copyNextSentence(b->title);
	copyNextSentence(b->modName);

	if (!strcmp(founderName, relayHoster))
		JoinBattle(b->id, relayPassword);
}

static void battleClosed(void)
{
	Battle *b = FindBattle(getNextInt());
	assert(b);
	if (!b)
		return;

	if (b == gMyBattle){
		if (gMyBattle->founder != &gMyUser)
			MyMessageBox("Leaving Battle", "The battle was closed by the host.");
		LeftBattle();
	}

	for (int i=0; i<b->nbParticipants; ++i)
		b->users[i]->user.battle = NULL;

	BattleList_CloseBattle(b);
	DelBattle(b);
}

static void broadcast(void)
{
	serverMsgBox();
}

static void channel(void)
{
	const char *channame = getNextWord();
	const char *usercount = getNextWord();
	const char *description = command;
	ChannelList_AddChannel(channame, usercount, description);
}

static void channelTopic(void)
{
	const char *chanName = getNextWord();
	/* const char *username =  */getNextWord();
	/* const char *unixTime =  */getNextWord();
	char *s;
	while ((s = strstr(command, "\\n")))
		*(uint16_t *)s = *(uint16_t *)(char [2]){'\r', '\n'};
	Chat_Said(GetChannelChat(chanName), NULL, CHAT_TOPIC, command);
}

static void clientBattleStatus(void)
{
	User *u = FindUser(getNextWord());
	uint32_t bs = getNextInt();
	UpdateBattleStatus((UserOrBot *)u, (bs & ~INTERNAL_MASK) | (u->battleStatus & INTERNAL_MASK), getNextInt());
}

static void clients(void)
{
	const char *chanName = getNextWord();
	HWND window = GetChannelChat(chanName);
	for (const char *userName; *(userName = getNextWord()); )
		ChatWindow_AddUser(window, FindUser(userName));
}

static void clientStatus(void)
{
	User *u = FindUser(getNextWord());
	if (!u)
		return;
	uint8_t oldStatus = u->clientStatus;
	u->clientStatus = getNextInt();
	// UserList_AddUser(u);
	if (gMyBattle && u->battle == gMyBattle)
		BattleRoom_UpdateUser((void *)u);
	/* UpdateUser(u); */
	if (!u->battle)
		return;
	if (u == &gMyUser)
		gLastClientStatus = (u->clientStatus & ~CS_INGAME_MASK) | (gLastClientStatus & CS_INGAME_MASK);
	if (u == u->battle->founder && (oldStatus ^ u->clientStatus) & CS_INGAME_MASK)
		BattleList_UpdateBattle(u->battle);

	if (gMyBattle == u->battle
			&& !(oldStatus & CS_INGAME_MASK)
			&& u->clientStatus & CS_INGAME_MASK
			&& u == u->battle->founder
			&& u != &gMyUser)
		LaunchSpring();
}

static void denied(void)
{
	Disconnect();
	MyMessageBox("Connection denied", command);
}

static void forceQuitBattle(void)
{
	static char buff[sizeof(" has kicked you from the current battle") + MAX_TITLE];
	sprintf(buff, "%s has kicked you from the current battle.", gMyBattle->founder->name);
	MyMessageBox("Leaving battle", buff);
}

static void join(void)
{
	const char *chanName = getNextWord();
	ChatWindow_AddTab(GetChannelChat(chanName));
}

static void joinBattle(void)
{
	Battle *b = FindBattle(getNextInt());
	JoinedBattle(b, getNextInt());
	timeJoinedBattle = GetTickCount();
}

static void joinBattleFailed(void)
{
	BattleRoom_Hide();
	MyMessageBox("Failed to join battle", command);
}

static void joined(void)
{
	const char *chanName = getNextWord();
	const char *userName = getNextWord();
	ChatWindow_AddUser(GetChannelChat(chanName), FindUser(userName));
	if (gSettings.flags & (1<<DEST_CHANNEL))
		Chat_Said(GetChannelChat(chanName), userName, CHAT_SYSTEM, "has joined the channel");
}

static void joinedBattle(void)
/* JOINEDBATTLE battleID userName [scriptPassword] */
{
	Battle *b = FindBattle(getNextInt());
	User *u = FindUser(getNextWord());
	assert(u && b);
	if (!u || !b)
		return;
	u->battle = b;
	free(u->scriptPassword);
	u->scriptPassword = strdup(getNextWord());

	int i=1; //Start at 1 so founder is first
	while (i<b->nbParticipants - b->nbBots && strcmpi(b->users[i]->name, u->name) < 0)
		++i;
	for (int j=b->nbParticipants; j>i; --j)
		b->users[j] = b->users[j-1];
	b->users[i] = (void *)u;
	++b->nbParticipants;
	u->battleStatus = 0;
	BattleList_UpdateBattle(b);
	/* UpdateUser(u); */

	if (b == gMyBattle){
		if (gSettings.flags & (1<<DEST_BATTLE))
			Chat_Said(GetBattleChat(), u->name, CHAT_SYSTEM, "has joined the battle");
	}
}

static void joinFailed(void)
{
	HWND chanWindow = GetChannelChat(getNextWord());
	if (chanWindow){
		SendMessage(chanWindow, WM_CLOSE, 0, 0);
		MyMessageBox("Couldn't join channel", command);
	}
}

static void left(void)
{
	const char *chanName = getNextWord();
	const char *userName = getNextWord();
	if (gSettings.flags & (1<<DEST_CHANNEL))
		Chat_Said(GetChannelChat(chanName), userName, CHAT_SYSTEM, "has left the channel");
	ChatWindow_RemoveUser(GetChannelChat(chanName), FindUser(userName));
}

static void leftBattle(void)
{
	Battle *b = FindBattle(getNextInt()); //Battle Unused
	User *u = FindUser(getNextWord());
	assert(b && u && b == u->battle);
	if (!u || !b)
		return;

	u->battle = NULL;
	int i=1; //Start at 1, we won't remove founder here
	for (; i < b->nbParticipants; ++i)
		if (&b->users[i]->user == u)
			break;
	assert(i < b->nbParticipants);
	if (i >= b->nbParticipants)
		return;
	--b->nbParticipants;
	for (;i < b->nbParticipants; ++i)
		b->users[i] = b->users[i + 1];

	if (u == &gMyUser)
		LeftBattle();

	/* UpdateUser(u); */
	BattleList_UpdateBattle(b);

	if (b == gMyBattle){
		if (u->battleStatus & MODE_MASK && BattleRoom_IsAutoUnspec())
			SetBattleStatus(&gMyUser, MODE_MASK, MODE_MASK);
		if (gSettings.flags & (1<<DEST_BATTLE)){
			Chat_Said(GetBattleChat(), u->name, CHAT_SYSTEM, "has left the battle");
			BattleRoom_RemoveUser((void *)u);
		}
	}
}

static void loginInfoEnd(void)
{
	OpenDefaultChannels();
	BattleList_OnEndLoginInfo();
	MainWindow_ChangeConnect(CONNECTION_ONLINE);
	/* SendToServer("SAYPRIVATE RelayHostManagerList !listmansrc\messages.c */
}

static void messageOfTheDay(void)
{
	Chat_Said(GetServerChat(), NULL, 0, command);
}

static void openBattle(void)
{
	// OPENBATTLE BATTLE_ID
	Battle *b = FindBattle(getNextInt());
	JoinedBattle(b, 0);
}

static void registrationAccepted(void)
{
	extern void login(void);
	login();
	MyMessageBox("Registration accepted", "Logging in now.");
}

static void removeBot(void)
{
	// REMOVEBOT BATTLE_ID name
	__attribute__((unused))
	char *battleID = getNextWord();
	assert(atoi(battleID) == gMyBattle->id);
	DelBot(getNextWord());
}

static void removeStartRect(void)
{
	memset(&gBattleOptions.startRects[getNextInt()], 0, sizeof(typeof(*gBattleOptions.startRects)));
	BattleRoom_StartPositionsChanged();
}

static void removeUser(void)
{
	User *u = FindUser(getNextWord());
	assert(u);
	if (!u)
		return;
	if (gSettings.flags & (1<<DEST_SERVER))
		Chat_Said(GetServerChat(), u->name, CHAT_SYSTEM, "has logged off");
	// TODO:
	// if (u->chatWindow)
	// Chat_Said(u->chatWindow, u->name, CHAT_SYSTEM, "has logged off");
	ChatWindow_RemoveUser(GetServerChat(), u);
	assert(!u->battle);
	DelUser(u);
}

static void requestBattleStatus(void)
{
	gMyUser.battleStatus &= ~MODE_MASK;
	battleInfoFinished = 1;
	SetBattleStatus(&gMyUser, GetNewBattleStatus(), MODE_MASK | TEAM_MASK | ALLY_MASK | READY_MASK);
	extern void resizePlayerListTabs(void);
	resizePlayerListTabs();
}

static void ring(void)
{
	Ring();
}

static void said(void)
{
	const char *chanName = getNextWord();
	const char *username = getNextWord();
	const char *text = command;
	Chat_Said(GetChannelChat(chanName), username, 0, text);
}

static void saidBattle(void)
{
	const char *username = getNextWord();
	char *text = command;

	char ingameUserName[MAX_NAME_LENGTH_NUL];

	if (gBattleOptions.hostType == HOST_SPADS
			&& !strcmp(username, gMyBattle->founder->name)
			&& sscanf(text, "<%" STRINGIFY(MAX_NAME_LENGTH_NUL) "[^>]> ", ingameUserName) == 1){
		text += 3 + strlen(ingameUserName);
		Chat_Said(GetBattleChat(), ingameUserName, CHAT_INGAME, text);
		return;
	}

	if (gBattleOptions.hostType == HOST_SPRINGIE
			&& !strcmp(username, gMyBattle->founder->name)
			&& text[0] == '['){

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

	Chat_Said(GetBattleChat(), username, 0, text);
}

static void saidBattleEx(void)
{
	const char *username = getNextWord();
	const char *text = command;

	// Check for autohost
	// welcome message is configurable, but chanceOfAutohost should usually be between 5 and 8.
	// a host saying "hi johnny" in the first 2 seconds will only give score of 3.
	if (gMyBattle && !strcmp(username, gMyBattle->founder->name) && GetTickCount() - timeJoinedBattle < 10000){
		int chanceOfAutohost = 0;
		chanceOfAutohost += GetTickCount() - timeJoinedBattle < 2000;
		chanceOfAutohost += text[0] == '*' && text[1] == ' ';
		chanceOfAutohost += StrStrIA(text, "hi ") != NULL;
		chanceOfAutohost += StrStrIA(text, "welcome ") != NULL;
		chanceOfAutohost += strstr(text, gMyUser.name) != NULL;
		chanceOfAutohost += strstr(text, username) != NULL;
		chanceOfAutohost += strstr(text, "!help") != NULL;

		char saidSpads = strstr(text, "SPADS") != NULL;
		char saidSpringie = strstr(text, "Springie") != NULL;
		chanceOfAutohost += saidSpads;
		chanceOfAutohost += saidSpringie;

		if (chanceOfAutohost > 3){
			if (saidSpads)
				gBattleOptions.hostType = HOST_SPADS;
			else if (saidSpringie)
				gBattleOptions.hostType = HOST_SPRINGIE;
			else{
				gLastAutoMessage = GetTickCount();
				SendToServer("SAYPRIVATE %s !version\nSAYPRIVATE %s !springie", username, username);
			}
		}
	}


	if (gBattleOptions.hostType == HOST_SPRINGIE){

		// Check for callvote:
		// "Do you want to apply options " + wordFormat + "? !vote 1 = yes, !vote 2 = no"
		// "Do you want to remove current boss " + ah.BossName + "? !vote 1 = yes, !vote 2 = no"
		if (!memcmp("Do you want to ", command, sizeof("Do you want to ") - 1)){
			char *commandStart = command + sizeof("Do you want to ") - 1;
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
		if (!memcmp("vote successful", command, sizeof("vote successful") - 1)
				|| !memcmp("not enough votes", command, sizeof("not enough votes") - 1)
				|| !memcmp(" poll cancelled", command, sizeof(" poll cancelled") - 1))
			BattleRoom_VoteEnded();
	}

	if (gBattleOptions.hostType == HOST_SPADS
			&& command[0] == '*' && command[1] == ' '){

		// Check for callvote:
		// "$user called a vote for command \"".join(" ",@{$p_params}."\" [!vote y, !vote n, !vote b]"
		if (!memcmp(" called a vote for command ", strchr(command + 2, ' ') ?: "", sizeof(" called a vote for command ") - 1)){
			char *commandStart = strchr(command, '"');
			if (commandStart){
				++commandStart;
				char *commandEnd = strchr(commandStart, '"');
				if (commandEnd){
					commandStart[0] = toupper(commandStart[0]);
					commandEnd[0] = '\0';
					BattleRoom_VoteStarted(commandStart);
					commandStart[0] = tolower(commandStart[0]);
					commandEnd[0] = '"';
				}
			}
		}

		// Check for endvote:
		if (!memcmp("* Vote for command \"", command, sizeof("* Vote for command \"") - 1)
				|| !memcmp("* Vote cancelled by ", command, sizeof("* Vote cancelled by ") - 1))
			BattleRoom_VoteEnded();
	}			

	Chat_Said(GetBattleChat(), username, CHAT_EX, text);
}

static void saidEx(void)
{
	const char *chanName = getNextWord();
	const char *username = getNextWord();
	const char *text = command;
	Chat_Said(GetChannelChat(chanName), username, CHAT_EX, text);
}

static void saidPrivate(void)
{
	const char *username = getNextWord();

	// Get list of relayhost managers
	if (!strcmp(username, "RelayHostManagerList") && !memcmp(command, "managerlist ", sizeof("managerlist ") - 1)){
		strsep(&command, " ");
		for (const char *c; (c = strsep(&command, " \t\n"));){
			User *u = FindUser(c);
			if (u){
				gRelayManagers = realloc(gRelayManagers, (gRelayManagersCount+1) * sizeof(*gRelayManagers));
				gRelayManagers[gRelayManagersCount++] = u->name;
			}
		}

		// If we are starting a relayhost game, then manager sends the name of the host to join:
	} else if (!strcmp(username, relayManager)){
		strcpy(relayHoster, command);
		*relayManager = '\0';

		// Relayhoster sending a users script password:
	} else if (!strcmp(username, relayHoster) && !memcmp(command, "JOINEDBATTLE ", sizeof("JOINEDBATTLE ") - 1)){
		command += sizeof("JOINEDBATTLE ") - 1;
		getNextWord();
		User *u = FindUser(getNextWord());
		if (u) {
			free(u->scriptPassword);
			u->scriptPassword = strdup(getNextWord());
		}

		// Zero-K juggler sends matchmaking command "!join <host>"
	} else if (gMyBattle && !strcmp(username, gMyBattle->founder->name) && !memcmp(command, "!join ", sizeof("!join ") - 1)){
		User *u = FindUser(command + sizeof("!join ") - 1);
		if (u && u->battle)
			JoinBattle(u->battle->id, NULL);

		// Check for pms that identify an autohost
	} else if (gMyBattle && !strcmp(username, gMyBattle->founder->name) && !memcmp(username, command, strlen(username))){

		// Response to "!springie":
		// "PlanetWars (Springie 2.2.0) running for 10.00:57:00"
		if (!memcmp(command + strlen(username), " (Springie ", sizeof(" (Springie ") - 1)
				&& strstr(command, " running for "))
			gBattleOptions.hostType = HOST_SPRINGIE;

		// Response to "!version":
		// "[TERA]DSDHost2 is running SPADS v0.9.10c (auto-update: testing), with following components:"
		else if (!memcmp(command + strlen(username), " is running SPADS v", sizeof(" is running SPADS v") - 1)
				&& strstr(command, ", with following components:"))
			gBattleOptions.hostType = HOST_SPADS;
		else
			goto normal;

		// Normal chat message:
	} else normal:{
		User *u = FindUser(username);
		if (!u || u->ignore)
			return;
		HWND window = GetPrivateChat(u);
		Chat_Said(window, username, 0, command);
		if (!gMyBattle || strcmp(username, gMyBattle->founder->name) || GetTickCount() - gLastAutoMessage > 2000)
			ChatWindow_SetActiveTab(window);
	}

}

static void sayPrivate(void)
{
	char *username = getNextWord();
	char *text = command;
	User *u = FindUser(username);
	if (u)
		Chat_Said(GetPrivateChat(u), gMyUser.name, 0, text);
}

static void serverMsg(void)
{
	messageOfTheDay();
}

static void serverMsgBox(void)
{
	MyMessageBox("Message from the server", command);
}

static void setScriptTags(void)
{
	SetScriptTags(command);
}

static void TASServer(void)
{
	getNextWord(); //= serverVersion
	const char *serverSpringVersion = getNextWord();
	const char *mySpringVersion = _GetSpringVersion();
	*strchr(serverSpringVersion, '.') = '\0';
	if (strcmp(serverSpringVersion, mySpringVersion)){
		char buff[128];
		sprintf(buff, "Server requires %s.\nYou are using %s.\n", serverSpringVersion, mySpringVersion);
		MyMessageBox("Wrong Spring version", buff);
		Disconnect();
		return;
	}
	gUdpHelpPort = getNextInt();
}

static void updateBattleInfo(void)
{
	Battle *b = FindBattle(getNextInt());
#ifndef UNSAFE
	if (!b)
		return;
#endif

	uint32_t lastMapHash = b->mapHash;

	b->nbSpectators = getNextInt();
	b->locked = getNextInt();
	b->mapHash = getNextInt();
	copyNextSentence(b->mapName);

	if (b == gMyBattle && (b->mapHash != lastMapHash || !gMapHash))
		ChangedMap(b->mapName);

	BattleList_UpdateBattle(b);
}

static void updateBot(void)
{
	// UPDATEBOT BATTLE_ID name battlestatus teamcolor
	__attribute__((unused))
	char *battleID = getNextWord();
	assert(atoi(battleID) == gMyBattle->id);
	char *name = getNextWord();
	for (int i=gMyBattle->nbParticipants - gMyBattle->nbBots; i<gMyBattle->nbParticipants; ++i){
		struct Bot *s = &gMyBattle->users[i]->bot;
		if (!strcmp(name, s->name)){
			uint32_t bs = getNextInt() | AI_MASK | MODE_MASK;
			UpdateBattleStatus((UserOrBot *)s, bs, getNextInt());
			return;
		}
	}
	assert(0);
}
