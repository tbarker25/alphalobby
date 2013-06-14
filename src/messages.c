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

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>
#include <Shlwapi.h>

#include "battle.h"
#include "battlelist.h"
#include "battleroom.h"
#include "channellist.h"
#include "chat.h"
#include "chat_window.h"
#include "client.h"
#include "client_message.h"
#include "common.h"
#include "countrycodes.h"
#include "dialogboxes.h"
#include "host_relay.h"
#include "host_spads.h"
#include "host_springie.h"
#include "mainwindow.h"
#include "minimap.h"
#include "mybattle.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "user.h"
#include "userlist.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

typedef struct Command {
	char name[20];
	void (*func)(void);
} Command;

static void accepted              (void);
static void add_bot               (void);
static void add_user              (void);
static void add_start_rect        (void);
static void agreement             (void);
static void agreement_end         (void);
static void battle_opened         (void);
static void battle_closed         (void);
static void broadcast             (void);
static void channel               (void);
static void channel_topic         (void);
static void client_battle_status  (void);
static void clients               (void);
static void client_status         (void);
static void denied                (void);
static void force_quit_battle     (void);
static void join                  (void);
static void join_battle           (void);
static void join_battle_failed    (void);
static void joined                (void);
static void joined_battle         (void);
static void join_failed           (void);
static void left                  (void);
static void left_battle           (void);
static void login_info_end        (void);
static void message_of_the_day    (void);
static void open_battle           (void);
static void registration_accepted (void);
static void remove_bot            (void);
static void remove_start_rect     (void);
static void remove_user           (void);
static void request_battle_status (void);
static void ring                  (void);
static void said                  (void);
static void said_battle           (void);
static void said_battle_ex        (void);
static void said_ex               (void);
static void said_private          (void);
static void say_private           (void);
static void server_msg            (void);
static void server_message_box    (void);
static void set_script_tags       (void);
static void TAS_server            (void);
static void update_battle_info    (void);
static void update_bot            (void);

static FILE *g_agreement_file;
static DWORD g_time_battle_joined;
static char *g_command;

static const Command SERVER_COMMANDS[] = {
	{"ACCEPTED", accepted},
	{"ADDBOT", add_bot},
	{"ADDSTARTRECT", add_start_rect},
	{"ADDUSER", add_user},
	{"AGREEMENT", agreement},
	{"AGREEMENTEND", agreement_end},
	{"BATTLECLOSED", battle_closed},
	{"BATTLEOPENED", battle_opened},
	{"BROADCAST", broadcast},
	{"CHANNEL", channel},
	{"CHANNELTOPIC", channel_topic},
	{"CLIENTBATTLESTATUS", client_battle_status},
	{"CLIENTS", clients},
	{"CLIENTSTATUS", client_status},
	{"DENIED", denied},
	{"FORCEQUITBATTLE", force_quit_battle},
	{"JOIN", join},
	{"JOINBATTLE", join_battle},
	{"JOINBATTLEFAILED", join_battle_failed},
	{"JOINED", joined},
	{"JOINEDBATTLE", joined_battle},
	{"JOINFAILED", join_failed},
	{"LEFT", left},
	{"LEFTBATTLE", left_battle},
	{"LOGININFOEND", login_info_end},
	{"MOTD", message_of_the_day},
	{"OPENBATTLE", open_battle},
	{"REGISTRATIONACCEPTED", registration_accepted},
	{"REMOVEBOT", remove_bot},
	{"REMOVESTARTRECT", remove_start_rect},
	{"REMOVEUSER", remove_user},
	{"REQUESTBATTLESTATUS", request_battle_status},
	{"RING", ring},
	{"SAID", said},
	{"SAIDBATTLE", said_battle},
	{"SAIDBATTLEEX", said_battle_ex},
	{"SAIDEX", said_ex},
	{"SAIDPRIVATE", said_private},
	{"SAYPRIVATE", say_private},
	{"SERVERMSG", server_msg},
	{"SERVERMSGBOX", server_message_box },
	{"SETSCRIPTTAGS", set_script_tags},
	{"TASServer", TAS_server},
	{"UPDATEBATTLEINFO", update_battle_info},
	{"UPDATEBOT", update_bot},
};

static void
copy_next_word(char *buf) {
	size_t len = strcspn(g_command, " ");
	char *word = g_command;
	g_command += len + !!g_command[len];
	word[len] = '\0';
	memcpy(buf, word, len + 1);
}

static char *
get_next_word(void) {
	size_t len = strcspn(g_command, " ");
	char *word = g_command;
	g_command += len + !!g_command[len];
	word[len] = '\0';
	return word;
}

static int
get_next_int(void) {
	return atoi(get_next_word());
}

static void
copy_next_sentence(char *buf) {
	size_t len = strcspn(g_command, "\t");
	char *word = g_command;
	g_command += len + !!g_command[len];
	word[len] = '\0';
	memcpy(buf, word, len + 1);
}

void
Messages_handle(char *command)
{
	g_command = command;
	char *command_name = get_next_word();
	typeof(*SERVER_COMMANDS) *com =
		bsearch(command_name, SERVER_COMMANDS, LENGTH(SERVER_COMMANDS),
				sizeof(*SERVER_COMMANDS), (void *)strcmp);
	if (com)
		com->func();
}

static void
accepted(void)
{
	copy_next_word(g_my_user.name);
	strcpy(g_my_user.alias, UNTAGGED_NAME(g_my_user.name));
}


static void
add_bot(void)
{
	union {
		BattleStatus; uint32_t as_int;
	} bs;

	// ADDBOT BATTLE_ID name owner battlestatus teamcolor{AIDLL}
	__attribute__((unused))
	char *battle_id = get_next_word();
	assert(strtoul(battle_id, NULL, 10) == g_my_battle->id);
	char *name = get_next_word();
	User *owner = Users_find(get_next_word());
	assert(owner);
	bs.as_int = get_next_int();
	uint32_t color = get_next_int();
	Users_add_bot(name, owner, bs.BattleStatus, color, g_command);
}

static void
add_user(void)
{
	char *name = get_next_word();

	uint8_t country = Country_get_id(g_command);
	g_command += 3;

	uint32_t cpu = get_next_int();

	User *u = Users_new(get_next_int(), name);
	strcpy(u->name, name);
	u->country = country;
	u->cpu = cpu;

	Chat_add_user(Chat_get_server_window(), u);
	if (g_settings.flags & (1<<DEST_SERVER))
		Chat_said(Chat_get_server_window(), u->name, CHAT_SYSTEM, "has logged in");

	RelayHost_on_add_user(u->name);
}

static void
add_start_rect(void)
{
	typeof(*g_battle_options.start_rects) *rect = &g_battle_options.start_rects[get_next_int()];
	rect->left = get_next_int();
	rect->top = get_next_int();
	rect->right = get_next_int();
	rect->bottom = get_next_int();
	Minimap_on_start_position_change();
}

static void
agreement(void)
{
	g_agreement_file = g_agreement_file ?: tmpfile();
	fputs(g_command, g_agreement_file);
}

static void
agreement_end(void)
{
	rewind(g_agreement_file);
	SendMessage(g_main_window, WM_EXECFUNCPARAM, (WPARAM)CreateAgreementDlg, (LPARAM)g_agreement_file);
	g_agreement_file = NULL;
}

static void
battle_opened(void)
{
	Battle *b = Battles_new();

	b->id = get_next_int();
	b->type = get_next_int();
	b->nat_type = get_next_int();

	char founder_name[MAX_NAME_LENGTH_NUL];
	copy_next_word(founder_name);
	b->founder = Users_find(founder_name);
	assert(b->founder);
	b->user_len = 1;
	b->founder->battle = b;

	copy_next_word(b->ip);
	b->port = get_next_int();
	b->max_players = get_next_int();
	b->passworded = get_next_int();
	b->rank = get_next_int();
	b->map_hash = get_next_int();
	copy_next_sentence(b->map_name);
	copy_next_sentence(b->title);
	copy_next_sentence(b->mod_name);

	RelayHost_on_battle_opened(b);

	Chat_update_user(b->founder);

	BattleList_add_battle(b);
}

static void
battle_closed(void)
{
	Battle *b = Battles_find(get_next_int());
	assert(b);
	if (!b)
		return;

	if (b == g_my_battle){
		if (g_my_battle->founder != &g_my_user)
			MainWindow_msg_box("Leaving Battle", "The battle was closed by the host.");
		MyBattle_left_battle();
	}

	for (int i=0; i<b->user_len; ++i)
		b->users[i]->battle = NULL;

	BattleList_close_battle(b);
	Battles_del(b);
}

static void
broadcast(void)
{
	server_message_box();
}

static void
channel(void)
{
	const char *channame = get_next_word();
	const char *usercount = get_next_word();
	const char *description = g_command;
	ChannelList_add_channel(channame, usercount, description);
}

static void
channel_topic(void)
{
	const char *channel_name = get_next_word();
	/* const char *username =  */get_next_word();
	/* const char *unix_time =  */get_next_word();
	char *s;
	while ((s = strstr(g_command, "\\n")))
		*(uint16_t *)s = *(uint16_t *)(char [2]){'\r', '\n'};
	Chat_said(Chat_get_channel_window(channel_name), NULL, CHAT_TOPIC, g_command);
}

static void
client_battle_status(void)
{
	User *u;
	union {
		BattleStatus; uint32_t as_int;
	} bs;

	u = Users_find(get_next_word());
	bs.as_int = get_next_int();

	MyBattle_update_battle_status((UserOrBot *)u,
			bs.BattleStatus,
			get_next_int());
}

static void
clients(void)
{
	const char *channel_name = get_next_word();
	HWND window = Chat_get_channel_window(channel_name);
	for (const char *username; *(username = get_next_word()); )
		Chat_add_user(window, Users_find(username));
}

static void
client_status(void)
{
	union {
		ClientStatus; uint8_t as_int;
	} status;

	ClientStatus previous;
	User *u;

	u = Users_find(get_next_word());
	if (!u)
		return;

	status.as_int = get_next_int();
	previous = u->ClientStatus;
	u->ClientStatus = status.ClientStatus;

	if (u == &g_my_user)
		g_last_client_status = u->ClientStatus;

	if (g_my_battle && u->battle == g_my_battle)
		BattleRoom_update_user((void *)u);

	if (previous.ingame != u->ingame
			|| previous.away != u->away)
		Chat_update_user(u);

	if (!u->battle)
		return;

	if (previous.ingame != u->ingame
			&& u == u->battle->founder) {
		BattleList_update_battle(u->battle);

		if (g_my_battle == u->battle && u->ingame && u != &g_my_user)
			Spring_launch();
	}
}

static void
denied(void)
{
	Server_disconnect();
	MainWindow_msg_box("Connection denied", g_command);
}

static void
force_quit_battle(void)
{
	static char buf[sizeof(" has kicked you from the current battle") + MAX_TITLE];
	sprintf(buf, "%s has kicked you from the current battle.", g_my_battle->founder->name);
	MainWindow_msg_box("Leaving battle", buf);
}

static void
join(void)
{
	const char *channel_name = get_next_word();
	ChatWindow_add_tab(Chat_get_channel_window(channel_name));
}

static void
join_battle(void)
{
	Battle *b = Battles_find(get_next_int());
	MyBattle_joined_battle(b, get_next_int());
	g_time_battle_joined = GetTickCount();
}

static void
join_battle_failed(void)
{
	BattleRoom_hide();
	MainWindow_msg_box("Failed to join battle", g_command);
}

static void
joined(void)
{
	const char *channel_name = get_next_word();
	const char *username = get_next_word();
	Chat_add_user(Chat_get_channel_window(channel_name), Users_find(username));
	if (g_settings.flags & (1<<DEST_CHANNEL))
		Chat_said(Chat_get_channel_window(channel_name), username, CHAT_SYSTEM, "has joined the channel");
}

static void
joined_battle(void)
/* JOINEDBATTLE battle_id username [script_password] */
{
	Battle *b = Battles_find(get_next_int());
	User *u = Users_find(get_next_word());
	assert(u && b);
	if (!u || !b)
		return;
	u->battle = b;
	free(u->script_password);
	u->script_password = _strdup(get_next_word());

	int i=1; //Start at 1 so founder is first
	while (i<b->user_len - b->bot_len && _stricmp(b->users[i]->name, u->name) < 0)
		++i;
	for (int j=b->user_len; j>i; --j)
		b->users[j] = b->users[j-1];
	b->users[i] = (void *)u;
	++b->user_len;
	u->BattleStatus = (BattleStatus){0};
	BattleList_update_battle(b);
	Chat_update_user(u);

	if (b == g_my_battle){
		if (g_settings.flags & (1<<DEST_BATTLE))
			Chat_said(GetBattleChat(), u->name, CHAT_SYSTEM, "has joined the battle");
	}
}

static void
join_failed(void)
{
	HWND chan_window = Chat_get_channel_window(get_next_word());
	if (chan_window){
		SendMessage(chan_window, WM_CLOSE, 0, 0);
		MainWindow_msg_box("Couldn't join channel", g_command);
	}
}

static void
left(void)
{
	const char *channel_name = get_next_word();
	const char *username = get_next_word();
	if (g_settings.flags & (1<<DEST_CHANNEL))
		Chat_said(Chat_get_channel_window(channel_name), username, CHAT_SYSTEM, "has left the channel");
	Chat_on_left_battle(Chat_get_channel_window(channel_name), Users_find(username));
}

static void
left_battle(void)
{
	Battle *b = Battles_find(get_next_int()); //Battle Unused
	User *u = Users_find(get_next_word());
	assert(b && u && b == u->battle);
	if (!u || !b)
		return;

	u->battle = NULL;
	int i=1; //Start at 1, we won't remove founder here
	for (; i < b->user_len; ++i)
		if (&b->users[i]->user == u)
			break;
	assert(i < b->user_len);
	if (i >= b->user_len)
		return;
	--b->user_len;
	for (;i < b->user_len; ++i)
		b->users[i] = b->users[i + 1];

	if (u == &g_my_user)
		MyBattle_left_battle();

	Chat_update_user(u);
	BattleList_update_battle(b);

	if (b == g_my_battle) {
		if (u->mode && BattleRoom_is_auto_unspec()) {
			BattleStatus bs = g_last_battle_status;
			SetMyBattleStatus(bs);
		}
		if (g_settings.flags & (1<<DEST_BATTLE)){
			Chat_said(GetBattleChat(), u->name, CHAT_SYSTEM, "has left the battle");
			BattleRoom_on_left_battle((void *)u);
		}
	}
}

static void
login_info_end(void)
{
	Settings_open_default_channels();
	BattleList_on_end_login_info();
	MainWindow_change_connect(CONNECTION_ONLINE);
	/* Server_send("SAYPRIVATE RelayHostManagerList !listmansrc\messages.c */
}

static void
message_of_the_day(void)
{
	Chat_said(Chat_get_server_window(), NULL, 0, g_command);
}

static void
open_battle(void)
{
	// OPENBATTLE BATTLE_ID
	Battle *b = Battles_find(get_next_int());
	MyBattle_joined_battle(b, 0);
}

static void
registration_accepted(void)
{
	extern void login(void);
	login();
	MainWindow_msg_box("Registration accepted", "Logging in now.");
}

static void
remove_bot(void)
{
	// REMOVEBOT BATTLE_ID name
	__attribute__((unused))
	char *battle_id = get_next_word();
	assert(strtoul(battle_id, NULL, 10) == g_my_battle->id);
	Users_del_bot(get_next_word());
}

static void
remove_start_rect(void)
{
	memset(&g_battle_options.start_rects[get_next_int()], 0, sizeof(typeof(*g_battle_options.start_rects)));
	Minimap_on_start_position_change();
}

static void
remove_user(void)
{
	User *u = Users_find(get_next_word());
	assert(u);
	if (!u)
		return;
	if (g_settings.flags & (1<<DEST_SERVER))
		Chat_said(Chat_get_server_window(), u->name, CHAT_SYSTEM, "has logged off");
	// TODO:
	// if (u->chat_window)
	// Chat_said(u->chat_window, u->name, CHAT_SYSTEM, "has logged off");
	Chat_on_left_battle(Chat_get_server_window(), u);
	assert(!u->battle);
	Users_del(u);
}

static void
request_battle_status(void)
{
	g_battle_info_finished = 1;

	SetMyBattleStatus(MyBattle_new_battle_status());
	BattleRoom_resize_columns();
}

static void
ring(void)
{
	MainWindow_ring();
}

static void
said(void)
{
	const char *channel_name = get_next_word();
	const char *username = get_next_word();
	const char *text = g_command;
	Chat_said(Chat_get_channel_window(channel_name), username, 0, text);
}

static void
said_battle(void)
{
	const char *username = get_next_word();
	char *text = g_command;

	if (g_host_type && g_host_type->said_battle)
		g_host_type->said_battle(username, text);
	else
		Chat_said(GetBattleChat(), username, 0, text);
}

static void
said_battle_ex(void)
{
	const char *username = get_next_word();
	char *text = g_command;

	// Check for autohost
	// welcome message is configurable, but chance_of_autohost should usually be between 5 and 8.
	// a host saying "hi johnny" in the first 2 seconds will only give score of 3.
	if (g_my_battle && !strcmp(username, g_my_battle->founder->name) && GetTickCount() - g_time_battle_joined < 10000){
		int chance_of_autohost = 0;
		chance_of_autohost += GetTickCount() - g_time_battle_joined < 2000;
		chance_of_autohost += text[0] == '*' && text[1] == ' ';
		chance_of_autohost += StrStrIA(text, "hi ") != NULL;
		chance_of_autohost += StrStrIA(text, "welcome ") != NULL;
		chance_of_autohost += strstr(text, g_my_user.name) != NULL;
		chance_of_autohost += strstr(text, username) != NULL;
		chance_of_autohost += strstr(text, "!help") != NULL;

		char said_spads = strstr(text, "SPADS") != NULL;
		char said_springie = strstr(text, "Springie") != NULL;
		chance_of_autohost += said_spads;
		chance_of_autohost += said_springie;

		if (chance_of_autohost > 3){
			if (said_spads) {
				g_host_type = &HOST_SPADS;
			} else if (said_springie) {
				g_host_type = &HOST_SPRINGIE;
			} else {
				g_last_auto_message = GetTickCount();
				Server_send("SAYPRIVATE %s !version\nSAYPRIVATE %s !springie", username, username);
			}
		}
	}

	Chat_said(GetBattleChat(), username, CHAT_EX, text);
}

static void
said_ex(void)
{
	const char *channel_name = get_next_word();
	const char *username = get_next_word();
	const char *text = g_command;
	Chat_said(Chat_get_channel_window(channel_name), username, CHAT_EX, text);
}

static void
said_private(void)
{
	const char *username = get_next_word();

	if (RelayHost_on_private_message(username, g_command))
		return;

	User *user = Users_find(username);
	if (!user || user->ignore)
		return;

	// Zero-K juggler sends matchmaking g_command "!join <host>"
	if (g_my_battle
			&& user == g_my_battle->founder
			&& !memcmp(g_command, "!join ", sizeof("!join ") - 1)) {
		User *user = Users_find(g_command + sizeof("!join ") - 1);
		if (user && user->battle)
			JoinBattle(user->battle->id, NULL);
		return;
	}

	// Check for pms that identify an autohost
	if (g_my_battle && user == g_my_battle->founder
			&& strstr(g_command, username)
			&& strstr(g_command, "running")) {

		// Response to "!springie":
		// "PlanetWars (Springie 2.2.0) running for 10.00:57:00"
		if (strstr(g_command, "Springie")) {
			g_host_type = &HOST_SPRINGIE;
			return;
		}

		// Response to "!version":
		// "[TERA]DSDHost2 is running SPADS v0.9.10c (auto-update: testing), with following components:"
		if (strstr(g_command, "SPADS")) {
			g_host_type = &HOST_SPADS;
			return;
		}
	}

	// Normal chat message:
	HWND window = Chat_get_private_window(user);
	Chat_said(window, username, 0, g_command);
	if (!g_my_battle
			|| strcmp(username, g_my_battle->founder->name)
			|| GetTickCount() - g_last_auto_message > 2000)
		ChatWindow_set_active_tab(window);
}

static void
say_private(void)
{
	char *username = get_next_word();
	char *text = g_command;
	User *u = Users_find(username);
	if (u)
		Chat_said(Chat_get_private_window(u), g_my_user.name, 0, text);
}

static void
server_msg(void)
{
	message_of_the_day();
}

static void
server_message_box(void)
{
	MainWindow_msg_box("Message from the server", g_command);
}

static void
set_script_tags(void)
{
	MyBattle_append_script(g_command);
}

static void
TAS_server(void)
{
	get_next_word(); //= server_version
	const char *server_spring_version = get_next_word();
	const char *my_spring_version = Sync_spring_version();
	*strchr(server_spring_version, '.') = '\0';
	if (strcmp(server_spring_version, my_spring_version)){
		char buf[128];
		sprintf(buf, "Server requires %s.\nYou are using %s.\n", server_spring_version, my_spring_version);
		MainWindow_msg_box("Wrong Spring version", buf);
		Server_disconnect();
		return;
	}
	g_udp_help_port = get_next_int();
}

static void
update_battle_info(void)
{
	Battle *b = Battles_find(get_next_int());
#ifndef UNSAFE
	if (!b)
		return;
#endif

	uint32_t last_map_hash = b->map_hash;

	b->spectator_len = get_next_int();
	b->locked = get_next_int();
	b->map_hash = get_next_int();
	copy_next_sentence(b->map_name);

	if (b == g_my_battle && (b->map_hash != last_map_hash || !g_map_hash))
		Sync_on_changed_map(b->map_name);

	BattleList_update_battle(b);
}

static void
update_bot(void)
{
	// UPDATEBOT BATTLE_ID name battlestatus teamcolor
	__attribute__((unused))
		char *battle_id = get_next_word();
	assert(strtoul(battle_id, NULL, 10) == g_my_battle->id);
	char *name = get_next_word();
	for (int i=g_my_battle->user_len - g_my_battle->bot_len; i<g_my_battle->user_len; ++i){
		struct Bot *s = &g_my_battle->users[i]->bot;
		if (!strcmp(name, s->name)){
			/* uint32_t bs = get_next_int() | BS_AI | BS_MODE; */
			/* MyBattle_update_battle_status((UserOrBot *)s, bs, get_next_int()); */
			return;
		}
	}
	assert(0);
}
