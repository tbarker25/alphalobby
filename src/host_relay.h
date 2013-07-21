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

#ifndef HOST_RELAY_H
#define HOST_RELAY_H

struct Battle;
struct User;

void RelayHost_on_add_user(const struct User *);
void RelayHost_on_battle_opened(const struct Battle *);
bool RelayHost_on_private_message(const char *username, char *command);
void RelayHost_open_battle(const char *title, const char *password, const char *mod_name, const char *map_name, const char *manager);

extern const char **g_relay_managers;
extern int g_relay_manager_len;

void Relay_set_as_host(void);

#endif /* end of include guard: HOST_RELAY_H */
