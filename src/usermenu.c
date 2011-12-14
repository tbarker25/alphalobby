#include <assert.h>
#include <malloc.h>
#include "wincommon.h"
#include "data.h"
#include "sync.h"
#include "usermenu.h"
#include "chat.h"
#include "dialogboxes.h"
#include "settings.h"
#include "client_message.h"

void CreateUserMenu(union UserOrBot *s, HWND window)
{
	
	enum menuID {
		CHAT = 1, IGNORED, ALIAS, ID, ALLY, SIDE, COLOR, KICK,
			//MUST BE CONSECUTIVE:
			RING, SPEC, KICKBAN,
		LAST=KICKBAN,
		FLAG_TEAM = 0x0100, FLAG_ALLY = 0x0200, FLAG_SIDE = 0x0400,
		AI_FLAG = 0x0800, AI_OPTIONS_FLAG = 0x10000,
		
		TEAM_MENU = 1, ALLY_MENU, SIDE_MENU, AI_MENU,
	};

	HMENU menus[100];
	
	int lastMenu = AI_MENU;
	for (int i=0; i<=lastMenu; ++i)
		menus[i] = CreatePopupMenu();

	uint32_t battleStatus = s->battleStatus;
	// HMENU menus[0] = CreatePopupMenu(), menus[TEAM_MENU] = CreatePopupMenu(), menus[ALLY_MENU] = CreatePopupMenu(), menus[SIDE_MENU] = CreatePopupMenu();
	// HMENU menus[AI_MENU+100] = {};
	
	for (int i=0; i<16; ++i) {
		wchar_t buff[3];
		swprintf(buff, L"%d", i+1);
		AppendMenu(menus[TEAM_MENU], MF_CHECKED * (i == FROM_TEAM_MASK(battleStatus)), FLAG_TEAM | i, buff);
		AppendMenu(menus[ALLY_MENU], MF_CHECKED * (i == FROM_ALLY_MASK(battleStatus)), FLAG_ALLY | i, buff);
	}

	for (int i=0; *gSideNames[i]; ++i)
		AppendMenuA(menus[SIDE_MENU], MF_CHECKED * (i == FROM_SIDE_MASK(battleStatus)), FLAG_SIDE | i, gSideNames[i]);

	
	if (battleStatus & AI_MASK) {
		if (!(gBattleOptions.hostType & HOST_FLAG))
			goto cleanup;
		if (gBattleOptions.hostType != HOST_SP)
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[TEAM_MENU], L"Set ID");
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[ALLY_MENU], L"Set team");
		AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[SIDE_MENU], L"Set faction");
		AppendMenu(menus[0], 0, COLOR, L"Set color");
		AppendMenu(menus[0], 0, KICK, L"Remove bot");
		if (gBattleOptions.hostType == HOST_SP) {
			int i=0;
			void appendAi(const char *name, void *unused)
			{
				AppendMenuA(menus[AI_MENU], MF_CHECKED * !strcmp(s->bot.dll, name), AI_FLAG | i++, name);
			}
			ForEachAiName(appendAi, NULL);
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[AI_MENU ], L"Change ai");
		}
		if (s->bot.nbOptions)
			AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
		for (int i=0;i < s->bot.nbOptions; ++i) {
			Option2 *opt = &s->bot.options[i];
			switch (opt->type) {
			case opt_list:
				menus[++lastMenu] = CreatePopupMenu();
				for (int j=0; j<opt->nbListItems; ++j)
					AppendMenuA(menus[lastMenu], MF_CHECKED * (!strcmp(opt->listItems[j].key, opt->val)), AI_OPTIONS_FLAG | i | j<<8, opt->listItems[j].name);
				AppendMenuA(menus[0], MF_POPUP, (UINT_PTR)menus[lastMenu], opt->name);
				break;
			case opt_bool: case opt_number:
				AppendMenuA(menus[0], MF_CHECKED * (opt->type == opt_bool && opt->val[0] != '0'), AI_OPTIONS_FLAG | i, opt->name);
				break;
			default:
				break;
			}
		}
	} else if (&s->user != gMyUser && gBattleOptions.hostType != HOST_SP) {
		AppendMenu(menus[0], 0, CHAT, L"Private chat");
		AppendMenu(menus[0], s->user.ignore * MF_CHECKED, IGNORED, L"Ignore");
		AppendMenu(menus[0], 0, ALIAS, L"Set alias");
		if (gBattleOptions.hostType) {
			AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
			AppendMenu(menus[0], 0, RING, L"Ring");
			if (battleStatus & MODE_MASK)
				AppendMenu(menus[0], 0, SPEC, L"Force spectate");
			AppendMenu(menus[0], 0, KICK, L"Kick");
			if (gBattleOptions.hostType == HOST_SPADS)
				AppendMenu(menus[0], 0, KICKBAN, L"Kick and ban");
		}
		AppendMenu(menus[0], MF_SEPARATOR, 0, NULL);
		if (gBattleOptions.hostType) {
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[TEAM_MENU], L"Set ID");
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[ALLY_MENU], L"Set team");
			if (gBattleOptions.hostType == HOST_RELAY)
				AppendMenu(menus[0], 0, COLOR, L"Set color");
		}
	} else { //(u == gMyUser)
		if (battleStatus & MODE_MASK) {
			if (gBattleOptions.hostType != HOST_SP) {
				AppendMenu(menus[0], 0, SPEC, L"Spectate");
				AppendMenu(menus[0], MF_POPUP, (UINT_PTR)menus[TEAM_MENU], L"Set ID");
			}
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[ALLY_MENU], L"Set team");
			AppendMenu(menus[0], MF_POPUP, (UINT_PTR )menus[SIDE_MENU], L"Set faction");
			AppendMenu(menus[0], 0, COLOR, L"Set color");
		} else
			AppendMenu(menus[0], 0, SPEC, L"Unspectate");
	}
	SetMenuDefaultItem(menus[0], 1, 0);
	
	POINT pt;
	GetCursorPos(&pt);
	int clicked = TrackPopupMenuEx(menus[0], TPM_RETURNCMD, pt.x, pt.y, window, NULL);
	switch (clicked) {
	case 0:
		break;
	case CHAT:
		FocusTab(GetPrivateChat(&s->user));
		break;
	case IGNORED:
		s->user.ignore ^= 1;
		UpdateUser(&s->user);
		break;
	case ALIAS: {
		char title[128], buff[MAX_NAME_LENGTH_NUL];
		sprintf(title, "Set alias for %s", s->name);
		if (!GetTextDlg(title, strcpy(buff, s->name), sizeof(buff))) {
			strcpy(s->user.alias, buff);
			UpdateUser(&s->user);
		}
		} break;
	case COLOR:
		CreateColorDlg(s);
		break;
	case KICK: {
		printf("kicking %s %s\n", s->name, s->bot.dll);
		Kick(s);
		}break;
	case SPEC:
		if (&s->user == gMyUser) {
			SetBattleStatus(gMyUser, ~s->user.battleStatus, MODE_MASK);
			break;
		}
		//FALLTHROUGH:
	case RING:
		if (gBattleOptions.hostType & HOST_FLAG) {
			SendToServer("!%s %s", clicked == RING ? "RING" : "FORCESPECTATORMODE", s->name);
			break;
		}
		//FALLTHROUGH:
	case KICKBAN:
		assert(gBattleOptions.hostType == HOST_SPADS);
		SpadsMessageF("!%s %s", (char *[]){"ring", "spec", "kickban"}[clicked - SPEC], s->name);
		break;
	default:
		if (clicked & AI_FLAG) {
			size_t len = GetMenuStringA(menus[AI_MENU], clicked, NULL, 0, MF_BYCOMMAND);
			free(s->bot.dll);
			s->bot.dll = malloc(len+1);
			GetMenuStringA(menus[AI_MENU], clicked, s->bot.dll, len+1, MF_BYCOMMAND);
			free(s->bot.options);
			s->bot.nbOptions = UnitSync_GetSkirmishAIOptionCount(s->bot.dll);
			s->bot.options = calloc(s->bot.nbOptions, sizeof(*s->bot.options));
			UnitSync_GetOptions(s->bot.options, s->bot.nbOptions);
		} else if (clicked & AI_OPTIONS_FLAG) {
			Option2 *opt= &s->bot.options[clicked & 0xFF];
			switch (opt->type) {
			case opt_list:
				strcpy(opt->val, opt->listItems[clicked >> 8 & 0xFF].key);
				break;
			case opt_number:
				GetTextDlg(opt->name, opt->val, opt->val - opt->extraData + sizeof(opt->extraData));
				break;
			case opt_bool:
				opt->val[0] ^= '0' ^ '1';
				break;
			default:
				break;
			}
		} else
			SetBattleStatus(s,
					(clicked & ~(FLAG_TEAM | FLAG_ALLY | FLAG_SIDE)) << (clicked & FLAG_TEAM ? TEAM_OFFSET : clicked & FLAG_ALLY ? ALLY_OFFSET : SIDE_OFFSET),
					clicked & FLAG_TEAM ? TEAM_MASK : clicked & FLAG_ALLY ? ALLY_MASK : SIDE_MASK);

		break;
	}
	cleanup:

	for (int i=0; i<=lastMenu; ++i)
		DestroyMenu(menus[i]);
}
