#pragma once

void SendToServer(const char *format, ...);
#define SpadsMessageF(format, ...)\
	(gLastAutoMessage = GetTickCount(), SendToServer("SAYPRIVATE %s " format, gMyBattle->founder->name, ## __VA_ARGS__))

void JoinBattle(uint32_t id, const char *password);

union UserOrBot;
void SetBattleStatusAndColor(union UserOrBot *s, uint32_t orMask, uint32_t nandMask, uint32_t color);
#define SetBattleStatus(_s, _orMask, _nandMask)\
	SetBattleStatusAndColor((union UserOrBot *)(_s), (_orMask), (_nandMask), -1)
#define SetColor(_s, color)\
	SetBattleStatusAndColor((union UserOrBot *)(_s), 0, 0, (color))
void Kick(union UserOrBot *);
	
void SetClientStatus(uint8_t s, uint8_t mask);
void RequestChannels(void);
void LeaveBattle(void);
void JoinChannel(const char *chanName, int focus);
void LeaveChannel(const char *chanName);
void RenameAccount(const char *newUsername);
void ChangePassword(const char *oldPassword, const char *newPassword);
void RegisterAccount(const char *username, const char *password);
bool Autologin(void);
void Login(const char *username, const char *password);
void OpenBattle(const char *title, const char *password, const char *modName, const char *mapName, uint16_t port);
void OpenRelayBattle(const char *title, const char *password, const char *modName, const char *mapName, const char *manager);
void ConfirmAgreement(void);
void RequestIngameTime(const char *username);
void ChangeMap(const char *mapName);

extern char relayCmd[1024], relayHoster[1024], relayManager[1024], relayPassword[1024];
extern int lastStatusUpdate, gLastAutoMessage;
void LeftBattle(void);
