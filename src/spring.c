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
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>

#include "battle.h"
#include "tasserver.h"
#include "common.h"
#include "countrycodes.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "settings.h"
#include "spring.h"
#include "user.h"

#define LAUNCH_SPRING(path)\
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)spring_proc, (void *)(_wcsdup(path)), 0, NULL);

static uint32_t WINAPI spring_proc(void * path);

static bool s_ingame;

static uint32_t WINAPI
spring_proc(void * path)
{
	PROCESS_INFORMATION process_info;
	STARTUPINFO startup_info = {0};
	startup_info.cb = sizeof startup_info;

	if (CreateProcess(NULL, path, NULL, NULL, 0, 0, NULL, NULL,
				&startup_info, &process_info)) {
		s_ingame = true;
		TasServer_send_my_client_status(g_last_client_status);
		#ifdef NDEBUG
		WaitForSingleObject(process_info.hProcess, INFINITE);
		#endif
		s_ingame = false;
		TasServer_send_my_client_status(g_last_client_status);
	} else
		MainWindow_msg_box("Failed to launch spring", "Check that the path is correct in 'Options>Lobby Preferences'.");

	free(path);
	return 0;
}

bool
Spring_is_ingame(void)
{
	return s_ingame;
}

void
Spring_launch(void)
//Script is unreadably compact because it might need to be sent to relayhost.
//This lets us send it in 1-2 packets, so it doesnt take 5 seconds like in springlobby.
{
	Battle *b = g_my_battle;
	assert(b);

	char buf[8192], *buffEnd=buf;

	#define APPEND_LINE(format, ...) \
		(buffEnd += sprintf(buffEnd, (format), ## __VA_ARGS__))

	APPEND_LINE("[GAME]{HostIP=%s;HostPort=%hu;MyPlayerName=%s;MyPasswd=%s;}",
			b->ip, b->port, g_my_user.name, g_my_user.script_password ?: "");

	wchar_t *script_path = Settings_get_data_dir(L"script.txt");
	FILE *fp = _wfopen(script_path, L"w");
	if (!fp) {
		MessageBox(NULL, L"Could not open script.txt for writing", L"Launch failed", MB_OK);
		fclose(fp);
		return;
	}
	fwrite(buf, 1, (size_t)(buffEnd - buf), fp);
	fclose(fp);
	wchar_t path[256];
	_swprintf(path, L"\"%hs\" \"%s\"", g_settings.spring_path, script_path);
	LAUNCH_SPRING(path);
	return;
}

void
Spring_launch_replay(const wchar_t *replay_name)
{
	wchar_t path[256];
	_swprintf(path, L"%hs demos/%s", g_settings.spring_path, replay_name);
	LAUNCH_SPRING(path);
}
