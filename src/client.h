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

#ifndef CLIENT_H
#define CLIENT_H

void PollServer(void);
void Connect(void (*onFinish)(void));
void Disconnect(void);
void SendToServer(const char *format, ...)
	__attribute__ ((format (ms_printf, 1, 2)));
// char * GetLocalIP(void);

enum ConnectionState {
	CONNECTION_OFFLINE,
	CONNECTION_CONNECTING,
	CONNECTION_ONLINE,
};

enum ConnectionState GetConnectionState(void)
	__attribute__((pure));

#endif /* end of include guard: CLIENT_H */
