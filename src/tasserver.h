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

#ifndef TASSERVER_H
#define TASSERVER_H

struct UserBot;
struct User;
struct BattleStatus;
struct ClientStatus;

enum ServerStatus {
	CONNECTION_OFFLINE,
	CONNECTION_CONNECTING,
	CONNECTION_ONLINE,
};

void TasServer_connect(void (*on_finish)(void));
void TasServer_disconnect(void);
void TasServer_poll(void);
enum ServerStatus TasServer_status(void) __attribute__((pure));

void TasServer_send_add_start_box(int i, int left, int top, int right, int bottom);
void TasServer_send_change_map(const char *map_name);
void TasServer_send_change_password(const char *old_password, const char *new_password);
void TasServer_send_confirm_agreement(void);
void TasServer_send_del_start_box(int i);
void TasServer_send_force_ally(const char *name, int ally);
void TasServer_send_force_color(const struct User *, uint32_t color);
void TasServer_send_force_team(const char *name, int team);
void TasServer_send_get_channels(void);
void TasServer_send_get_ingame_time(void);
void TasServer_send_join_battle(uint32_t id, const char *password);
void TasServer_send_join_channel(const char *channel_name);
void TasServer_send_kick(const struct UserBot *);
void TasServer_send_leave_channel(const char *channel_name);
void TasServer_send_login(const char *username, const char *password);
void TasServer_send_my_battle_status(struct BattleStatus);
void TasServer_send_my_client_status(struct ClientStatus status);
void TasServer_send_my_color(uint32_t color);
void TasServer_send_open_battle(const char *password, uint16_t port, const char *mod_name, const char *map_name, const char *description);
void TasServer_send_register(const char *username, const char *password);
void TasServer_send_rename(const char *new_username);
cdecl void TasServer_send_say_battle (const char *text, bool is_ex);
cdecl void TasServer_send_say_channel(const char *text, bool is_ex, const char *channel);
cdecl void TasServer_send_say_private(const char *text, bool is_ex, const struct User *);
void TasServer_send_script_tags(const char *script_tags);
void TasServer_send_set_map(const char *map_name);

extern int g_last_auto_message;
extern struct BattleStatus g_last_battle_status;
extern struct ClientStatus g_last_client_status;

#endif /* end of include guard: TASSERVER_H */
