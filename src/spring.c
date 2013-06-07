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
#include <stdio.h>
#include <malloc.h>

#include <windows.h>

#include "alphalobby.h"
#include "client_message.h"
#include "countrycodes.h"
#include "data.h"
#include "settings.h"

#define LAUNCH_SPRING(path)\
	CreateThread(NULL, 0, _launchSpring2, (LPVOID)(wcsdup(path)), 0, NULL);

static DWORD WINAPI _launchSpring2(LPVOID path)
{
	PROCESS_INFORMATION processInfo = {};
	if (CreateProcess(NULL, path, NULL, NULL, 0, 0, NULL, NULL, &(STARTUPINFO){.cb=sizeof(STARTUPINFO)}, &processInfo)) {
		SetClientStatus(CS_INGAME_MASK, CS_INGAME_MASK);
		#ifdef NDEBUG
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		#endif
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

	APPEND_LINE("[GAME]{HostIP=%s;HostPort=%hu;MyPlayerName=%s;MyPasswd=%s;}",
			b->ip, b->port, gMyUser.name, gMyUser.scriptPassword ?: "");

	wchar_t *scriptPath = GetDataDir(L"script.txt");
	FILE *fp = _wfopen(scriptPath, L"w");
	if (!fp) {
		MessageBox(NULL, L"Could not open script.txt for writing", L"Launch failed", MB_OK);
		fclose(fp);
		return;
	}
	fwrite(buff, 1, buffEnd - buff, fp);
	fclose(fp);
	wchar_t path[128];
	_swprintf(path, L"\"%hs\" \"%s\"", gSettings.spring_path, scriptPath);
	LAUNCH_SPRING(path);
	return;
}

void LaunchReplay(const wchar_t *replayName)
{
	wchar_t path[MAX_PATH];
	_swprintf(path, L"%hs demos/%s", gSettings.spring_path, replayName);
	printf("path = \"%ls\"\n", path);
	LAUNCH_SPRING(path);
}
