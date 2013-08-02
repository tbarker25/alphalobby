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
#include <stdio.h>

#include <windows.h>
#include <Shlwapi.h>

#include "battle.h"
#include "battlelist.h"
#include "battleroom.h"
#include "channellist.h"
#include "chatbox.h"
#include "chattab.h"
#include "common.h"
#include "countrycodes.h"
#include "dialogs/agreementdialog.h"
#include "host_relay.h"
#include "host_spads.h"
#include "host_springie.h"
#include "mainwindow.h"
#include "messages.h"
#include "minimap.h"
#include "mybattle.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "tasserver.h"
#include "user.h"
#include "userlist.h"

#define LENGTH(x) (sizeof x / sizeof *x)

typedef struct Command {
	const char name[20];
	void (*const func)(void);
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
static void said_private_ex       (void);
static void say_private           (void);
static void say_private_ex        (void);
static void server_msg            (void);
static void server_message_box    (void);
static void set_script_tags       (void);
static void tas_server            (void);
static void update_battle_info    (void);
static void update_bot            (void);

static uint32_t get_next_int                (void);
static char *   get_next_word               (void);
static char *   get_next_sentence           (void);
static void     strip_irc_control_sequences (char *);

static FILE     *agreement_file;
static uint32_t  time_battle_joined;
static char     *command;

static const Command SERVER_COMMANDS[] = {
	{"ACCEPTED",             accepted},
	{"ADDBOT",               add_bot},
	{"ADDSTARTRECT",         add_start_rect},
	{"ADDUSER",              add_user},
	{"AGREEMENT",            agreement},
	{"AGREEMENTEND",         agreement_end},
	{"BATTLECLOSED",         battle_closed},
	{"BATTLEOPENED",         battle_opened},
	{"BROADCAST",            broadcast},
	{"CHANNEL",              channel},
	{"CHANNELTOPIC",         channel_topic},
	{"CLIENTBATTLESTATUS",   client_battle_status},
	{"CLIENTS",              clients},
	{"CLIENTSTATUS",         client_status},
	{"DENIED",               denied},
	{"FORCEQUITBATTLE",      force_quit_battle},
	{"JOIN",                 join},
	{"JOINBATTLE",           join_battle},
	{"JOINBATTLEFAILED",     join_battle_failed},
	{"JOINED",               joined},
	{"JOINEDBATTLE",         joined_battle},
	{"JOINFAILED",           join_failed},
	{"LEFT",                 left},
	{"LEFTBATTLE",           left_battle},
	{"LOGININFOEND",         login_info_end},
	{"MOTD",                 message_of_the_day},
	{"OPENBATTLE",           open_battle},
	{"REGISTRATIONACCEPTED", registration_accepted},
	{"REMOVEBOT",            remove_bot},
	{"REMOVESTARTRECT",      remove_start_rect},
	{"REMOVEUSER",           remove_user},
	{"REQUESTBATTLESTATUS",  request_battle_status},
	{"RING",                 ring},
	{"SAID",                 said},
	{"SAIDBATTLE",           said_battle},
	{"SAIDBATTLEEX",         said_battle_ex},
	{"SAIDEX",               said_ex},
	{"SAIDPRIVATE",          said_private},
	{"SAIDPRIVATEEX",        said_private_ex},
	{"SAYPRIVATE",           say_private},
	{"SAYPRIVATEEX",         say_private_ex},
	{"SERVERMSG",            server_msg},
	{"SERVERMSGBOX",         server_message_box },
	{"SETSCRIPTTAGS",        set_script_tags},
	{"TASServer",            tas_server},
	{"UPDATEBATTLEINFO",     update_battle_info},
	{"UPDATEBOT",            update_bot},
};

void
Messages_handle(char *message)
{
	const char *command_name;
	Command    *command;

	command = message;
	command_name = get_next_word();
	command = bsearch(command_name, SERVER_COMMANDS,
	    LENGTH(SERVER_COMMANDS), sizeof *SERVER_COMMANDS, (void *)strcmp);
	if (command)
		command->func();
}

static void
accepted(void)
{
	strcpy(g_my_user.name, command);
	g_my_user.alias = g_my_user.name;
}


/* ADDBOT BATTLE_ID name owner battlestatus teamcolor{AIDLL} */
static void
add_bot(void)
{
	union {
		BattleStatus;
		uint32_t as_int;
	} status;
	User       *owner;

	uint32_t    battle_id  = get_next_int();
	const char *name       = get_next_word();
	const char *owner_name = get_next_word();
	uint32_t    int_status = get_next_int();
	uint32_t    color      = get_next_int();
	const char *ai_dll     = command;

	owner         = Users_find(owner_name);
	status.as_int = int_status;

	if (!owner) {
		assert(0);
		return;
	}

	if (battle_id != g_my_battle->id) {
		assert(0);
		return;
	}

	Users_add_bot(name, owner, status.BattleStatus, color, ai_dll);
}

static void
add_user(void)
{
	User       *u;
	const char *name         = get_next_word();
	const char *country_code = get_next_word();
	uint32_t   cpu           = get_next_int();
	uint32_t   id            = get_next_int();

	u          = Users_new(id, name);
	u->country = Country_get_id(country_code);
	u->cpu     = cpu;
	RelayHost_on_add_user(u);
}

static void
add_start_rect(void)
{
	uint8_t  ally_number = (uint8_t )get_next_int();
	uint16_t left        = (uint16_t)get_next_int();
	uint16_t top         = (uint16_t)get_next_int();
	uint16_t right       = (uint16_t)get_next_int();
	uint16_t bottom      = (uint16_t)get_next_int();

	g_battle_options.start_rects[ally_number]
		= (StartRect){left, top, right, bottom};
	Minimap_on_start_position_change();
}

static void
agreement(void)
{
	agreement_file = agreement_file ?: tmpfile();
	fputs(command, agreement_file);
}

static void
agreement_end(void)
{
	rewind(agreement_file);
	SendMessage(g_main_window, WM_EXECFUNCPARAM,
	    (uintptr_t)AgreementDialog_create, (intptr_t)agreement_file);
	agreement_file = NULL;
}

static void
battle_opened(void)
{
	Battle *b;
	User   *founder;

	uint32_t    id           = get_next_int();
	BattleType  battle_type  = get_next_int();
	NatType     nat_type     = get_next_int();
	const char *founder_name = get_next_word();
	const char *ip           = get_next_word();
	uint16_t    port         = (uint16_t)get_next_int();
	uint8_t     max_players  = (uint8_t)get_next_int();
	bool        passworded   = get_next_int();
	uint8_t     rank         = (uint8_t)get_next_int();
	uint32_t    map_hash     = get_next_int();
	const char *map_name     = get_next_sentence();
	const char *title        = get_next_sentence();
	const char *mod_name     = get_next_sentence();

	founder = Users_find(founder_name);

	if (!founder || strlen(ip) >= sizeof(b->ip)) {
		assert(0);
		return;
	}

	b              = Battles_new(id, title, mod_name);
	b->type        = battle_type;
	b->nat_type    = nat_type;
	b->founder     = founder;
	b->port        = port;
	b->max_players = max_players;
	b->passworded  = passworded;
	b->rank        = rank;
	b->map_hash    = map_hash;
	b->map_name    = _strdup(map_name);
	b->user_len    = 1;
	strcpy(b->ip, ip);

	founder->battle = b;
	founder->next_in_battle = NULL;

	RelayHost_on_battle_opened(b);

	BattleList_add_battle(b);
}

static void
battle_closed(void)
{
	uint32_t  id;
	Battle   *b;

	id = get_next_int();
	b  = Battles_find(id);
	if (!b) {
		assert(0);
		return;
	}
	if (b == g_my_battle){
		if (g_my_battle->founder != &g_my_user)
			MainWindow_msg_box("Leaving Battle",
			    "The battle was closed by the host.");
		MyBattle_left_battle();
	}

	Battles_for_each_user(u, b)
		u->battle = NULL;

	BattleList_close_battle(b);
	free(b->map_name);
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
	const char *channame    = get_next_word();
	const char *usercount   = get_next_word();
	const char *description = command;

	ChannelList_add_channel(channame, usercount, description);
}

static void
channel_topic(void)
{
	const char *channel;

	channel = get_next_word();
	get_next_word(); /* username */
	get_next_word(); /* unix_time */

	for (char *s; (s = strstr(command, "\\n")); ) {
		s[0] = '\r';
		s[1] = '\n';
	}
	ChatTab_on_said_channel(channel, NULL, command, CHAT_TOPIC);
}

static void
client_battle_status(void)
{
	union {
		BattleStatus;
		uint32_t as_int;
	} status;
	User       *u;
	const char *username;
	uint32_t    color;

	username      = get_next_word();
	status.as_int = get_next_int();
	color         = get_next_int();

	u = Users_find(username);
	if (!u) {
		assert(0);
		return;
	}
	MyBattle_update_battle_status(u, status.BattleStatus, color);
}

static void
clients(void)
{
	const char *channel_name;

	channel_name = get_next_word();
	while (*command) {
		User       *u;
		const char *username;

		username = get_next_word();
		u = Users_find(username);
		assert(u);
		if (u)
			ChatTab_add_user_to_channel(channel_name, u);
	}
}

static void
client_status(void)
{
	union {
		ClientStatus;
		uint8_t as_int;
	} status;

	User         *u;
	const char   *username;
	ClientStatus  previous;

	username      = get_next_word();
	u             = Users_find(username);
	status.as_int = (uint8_t)get_next_int();

	if (!u) {
		assert(0);
		return;
	}

	previous = u->ClientStatus;
	u->ClientStatus = status.ClientStatus;

	if (u == &g_my_user)
		g_last_client_status = u->ClientStatus;

	if (g_my_battle && u->battle == g_my_battle)
		BattleRoom_update_user((void *)u);

	if (!u->battle)
		return;

	if (previous.ingame != u->ingame && u == u->battle->founder) {
		BattleList_update_battle(u->battle);
		if (g_my_battle == u->battle && u->ingame && u != &g_my_user)
			Spring_launch();
	}
}

static void
denied(void)
{
	TasServer_disconnect();
	MainWindow_msg_box("Connection denied", command);
}

static void
force_quit_battle(void)
{
	char buf[sizeof " has kicked you from the current battle." + MAX_NAME_LENGTH];
	sprintf(buf, "%s has kicked you from the current battle.", g_my_battle->founder->name);
	MainWindow_msg_box("Leaving battle", buf);
}

static void
join(void)
{
	const char *channel = get_next_word();
	ChatTab_focus_channel(channel);
}

static void
join_battle(void)
{
	uint32_t  id;
	Battle   *b;
	uint32_t  mod_hash;

	id       = get_next_int();
	mod_hash = get_next_int();
	b        = Battles_find(id);
	MyBattle_joined_battle(b, mod_hash);
	time_battle_joined = GetTickCount();
}

static void
join_battle_failed(void)
{
	BattleRoom_hide();
	MainWindow_msg_box("Failed to join battle", command);
}

static void
joined(void)
{
	User       *user;
	const char *channel;
	const char *username;

	channel  = get_next_word();
	username = get_next_word();
	user     = Users_find(username);
	if (!user) {
		assert(0);
		return;
	}
	ChatTab_on_said_channel(channel, user, "Has joined the channel",
	    CHAT_SYSTEM);
}

static void
joined_battle(void)
/* JOINEDBATTLE battle_id username [script_password] */
{
	User       *u;
	Battle     *b;
	uint32_t    id;
	const char *username;
	const char *password;

	id       = get_next_int();
	username = get_next_word();
	password = get_next_word();
	b        = Battles_find(id);
	u        = Users_find(username);

	if (!u || !b) {
		assert(0);
		return;
	}
	free(u->script_password);
	u->battle                  = b;
	u->script_password         = _strdup(password);
	u->next_in_battle          = b->founder->next_in_battle;
	b->founder->next_in_battle = u;
	u->BattleStatus            = (BattleStatus){0};
	++b->user_len;

	BattleList_update_battle(b);

	if (b == g_my_battle) {
		BattleRoom_said_battle(u->name, "has joined the battle",
		    CHAT_SYSTEM);
	}
}

static void
join_failed(void)
{
}

static void
left(void)
{
	User       *user;
	const char *channel;
	const char *username;

	channel  = get_next_word();
	username = get_next_word();
	user     = Users_find(username);
	if (!user) {
		assert(0);
		return;
	}

	ChatTab_on_said_channel(channel, user, "has left the channel",
	    CHAT_SYSTEM);
	ChatTab_remove_user_from_channel(channel, user);
}

static void
left_battle(void)
{
	Battle     *b;
	User       *u;
	const char *username;
	uint32_t    id;

	id       = get_next_int();
	username = get_next_word();
	b        = Battles_find(id);
	u        = Users_find(username);
	if (!u || !b) {
		assert(0);
		return;
	}

	u->battle = NULL;
	for (UserBot **u2 = &b->founder->next_in_battle; *u2; u2 = &(*u2)->next_in_battle) {
		if (*u2 == (UserBot *)u) {
			*u2 = u->next_in_battle;
			goto done;
		}
	}
	assert(0);
	done:

	--b->user_len;
	if (u == &g_my_user)
		MyBattle_left_battle();

	BattleList_update_battle(b);

	if (b == g_my_battle) {
		if (u->mode && !g_my_user.mode
		    && g_last_auto_unspec + 5000 < GetTickCount()
		    && BattleRoom_is_auto_unspec()) {
			BattleStatus bs;
			g_last_auto_unspec = GetTickCount();
			bs = g_last_battle_status;
			bs.mode = 1;
			TasServer_send_my_battle_status(bs);
		}
		BattleRoom_on_left_battle(u);
		BattleRoom_said_battle(u->name, "has left the battle",
		    CHAT_SYSTEM);
	}
}

static void
login_info_end(void)
{
	Settings_open_default_channels();
	BattleList_on_end_login_info();
	MainWindow_change_connect(CONNECTION_ONLINE);
}

static void
message_of_the_day(void)
{
}

static void
open_battle(void)
{
	Battle   *b;
	uint32_t  id;

	id = get_next_int();
	b  = Battles_find(id);
	MyBattle_joined_battle(b, 0);
}

static void
registration_accepted(void)
{
	TasServer_send_login(NULL, NULL);
	MainWindow_msg_box("Registration accepted", "Logging in now.");
}

static void
remove_bot(void)
{
	uint32_t    id;
	const char *bot_name;

	id       = get_next_int();
	bot_name = get_next_word();
	if (id != g_my_battle->id) {
		assert(0);
		return;
	}
	Users_del_bot(bot_name);
}

static void
remove_start_rect(void)
{
	uint8_t ally_number;

	ally_number = (uint8_t)get_next_int();
	g_battle_options.start_rects[ally_number] = (StartRect){0};
	Minimap_on_start_position_change();
}

static void
remove_user(void)
{
	char *username;

	username = get_next_word();
	Users_del(username);
}

static void
request_battle_status(void)
{
	g_battle_info_finished = 1;

	TasServer_send_my_battle_status(MyBattle_new_battle_status());
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
	User       *user;
	char       *text;
	const char *channel;
	const char *username;

	channel  = get_next_word();
	username = get_next_word();
	text     = command;
	user     = Users_find(username);
	if (!user) {
		assert(0);
		return;
	}
	strip_irc_control_sequences(text);
	ChatTab_on_said_channel(channel, user, text, CHAT_NORMAL);
}

static void
said_battle(void)
{
	User       *u;
	char       *text;
	const char *username;

	username = get_next_word();
	text     = command;
	u        = Users_find(username);
	if (!u) {
		assert(0);
		return;
	}
	strip_irc_control_sequences(text);
	MyBattle_said_battle(u, text, CHAT_NORMAL);
}

static void
said_battle_ex(void)
{
	User       *u;
	char       *text;
	const char *username;

	username = get_next_word();
	text     = command;
	u        = Users_find(username);
	if (!u) {
		assert(0);
		return;
	}
	strip_irc_control_sequences(text);

	/* Check for autohost */
	/* welcome message is configurable, but chance_of_autohost should usually be between 5 and 8. */
	/* a host saying "hi johnny" in the first 2 seconds will only give score of 3. */
	if (g_my_battle && u == g_my_battle->founder
	    && GetTickCount() - time_battle_joined < 10000) {
		int chance_of_autohost = 0;
		bool said_spads;
		bool said_springie;

		chance_of_autohost += GetTickCount() - time_battle_joined < 2000;
		chance_of_autohost += text[0] == '*' && text[1] == ' ';
		chance_of_autohost += StrStrIA(text, "hi ") != NULL;
		chance_of_autohost += StrStrIA(text, "welcome ") != NULL;
		chance_of_autohost += strstr(text, g_my_user.name) != NULL;
		chance_of_autohost += strstr(text, u->name) != NULL;
		chance_of_autohost += strstr(text, "!help") != NULL;

		said_spads = strstr(text, "SPADS") != NULL;
		said_springie = strstr(text, "Springie") != NULL;
		chance_of_autohost += said_spads;
		chance_of_autohost += said_springie;

		if (chance_of_autohost > 3) {
			if (said_spads) {
				Spads_set_as_host();

			} else if (said_springie) {
				Springie_set_as_host();

			} else {
				g_last_auto_message = GetTickCount();
				TasServer_send_say_private("!version", 0, u->name);
				TasServer_send_say_private("!springie", 0, u->name);
			}
		}
	}

	MyBattle_said_battle(u, command, CHAT_EX);
}

static void
said_ex(void)
{
	User       *user;
	char       *text;
	const char *channel;
	const char *username;

	channel  = get_next_word();
	username = get_next_word();
	text     = command;
	user     = Users_find(username);
	if (!user) {
		assert(0);
		return;
	}
	strip_irc_control_sequences(text);
	ChatTab_on_said_channel(channel, user, text, CHAT_EX);
}

static void
said_private(void)
{
	User *user;
	char *text;
	const char *username;

	username = get_next_word();
	text     = command;
	user     = Users_find(username);
	if (!user) {
		assert(0);
		return;
	}
	if (user->ignore)
		return;

	strip_irc_control_sequences(text);
	if (RelayHost_on_private_message(user->name, text))
		return;

	/* Zero-K juggler sends matchmaking text "!join <host>" */
	if (g_my_battle
			&& user == g_my_battle->founder
			&& !memcmp(text, "!join ", sizeof "!join " - 1)) {
		User *host = Users_find(text + sizeof "!join " - 1);
		if (host && host->battle)
			TasServer_send_join_battle(host->battle->id, NULL);
		return;
	}

	/* Check for pms that identify an autohost */
	if (g_my_battle && user == g_my_battle->founder
			&& strstr(text, user->name)
			&& strstr(text, "running")) {

		/* Response to "!springie": */
		/* "PlanetWars (Springie 2.2.0) running for 10.00:57:00" */
		if (strstr(text, "Springie")) {
			Springie_set_as_host();
			return;
		}

		/* Response to "!version": */
		/* "[TERA]DSDHost2 is running SPADS v0.9.10c (auto-update: testing), with following components:" */
		if (strstr(text, "SPADS")) {
			Spads_set_as_host();
			return;
		}
	}

	ChatTab_on_said_private(user, text, 0);
}

static void
said_private_ex(void)
{
	User       *user;
	char       *text;
	const char *username;

	username = get_next_word();
	text     = command;
	user     = Users_find(username);
	if (!user) {
		assert(0);
		return;
	}
	if (user->ignore)
		return;
	strip_irc_control_sequences(text);
	ChatTab_on_said_private(user, text, CHAT_EX);
}

static void
say_private(void)
{
	User       *u;
	char       *text;
	const char *username;

	username = get_next_word();
	text     = command;
	u        = Users_find(username);
	if (!u) {
		assert(0);
		return;
	}
	ChatTab_on_said_private(u, text, CHAT_SELF);
}

static void
say_private_ex(void)
{
	User       *u;
	char       *text;
	const char *username;

	username = get_next_word();
	text     = command;
	u        = Users_find(username);
	if (!u) {
		assert(0);
		return;
	}
	ChatTab_on_said_private(u, text, CHAT_SELFEX);
}

static void
server_msg(void)
{
	message_of_the_day();
}

static void
server_message_box(void)
{
	MainWindow_msg_box("Message from the server", command);
}

static void
set_script_tags(void)
{
	MyBattle_append_script(command);
}

static void
tas_server(void)
{
	const char *server_spring_version;
	const char *my_spring_version;

	get_next_word(); /* server_version */
	server_spring_version = get_next_word();
	my_spring_version = Sync_spring_version();
	*strchr(server_spring_version, '.') = '\0';
	if (strcmp(server_spring_version, my_spring_version)){
		char buf[128];

		sprintf(buf, "Server requires %s.\nYou are using %s.\n", server_spring_version, my_spring_version);
		MainWindow_msg_box("Wrong Spring version", buf);
		TasServer_disconnect();
		return;
	}
	g_udp_help_port = get_next_int();
}

static void
update_battle_info(void)
{
	Battle     *b;
	const char *map_name;
	uint32_t    last_map_hash;
	uint32_t    id;
	size_t      map_len;

	id = get_next_int();
	b  = Battles_find(id);
	if (!b) {
		assert(0);
		return;
	}

	b->spectator_len = (uint8_t)get_next_int();
	b->locked        = (uint8_t)get_next_int();
	last_map_hash    = b->map_hash;
	b->map_hash      = get_next_int();
	map_name         = command;
	map_len          = strlen(command);

	if (map_len > strlen(b->map_name)) {
		free(b->map_name);
		b->map_name = malloc(map_len + 1);
	}
	memcpy(b->map_name, map_name, map_len + 1);

	if (b == g_my_battle && (b->map_hash != last_map_hash))
		Sync_on_changed_map(b->map_name);

	BattleList_update_battle(b);
}

static void
update_bot(void)
{
	/* UPDATEBOT BATTLE_ID name battlestatus teamcolor */
}

static void
strip_irc_control_sequences(char *text)
{
	int   skips = 0;
	char *c     = text;

	for (; *c; ++c) {
		/* 0x03 changes color (next two chars specify which color) */
		if (*c == (char)0x03) {
			++c;
			skips += 2;

		} else if ((signed char)*c >= 0 && !isprint(*c))
			++skips;

		else
			c[-skips] = *c;
	}
	c[-skips] = '\0';
}

static char *
get_next_word(void) {
	size_t  len  = strcspn(command, " ");
	char   *word = command;

	command += len + !!command[len];
	word[len] = '\0';
	return word;
}

static uint32_t
get_next_int(void) {
	return (uint32_t)atol(get_next_word());
}

static char *
get_next_sentence(void) {
	size_t  len  = strcspn(command, "\t");
	char   *word = command;

	command += len + !!command[len];
	word[len] = '\0';
	return word;
}
