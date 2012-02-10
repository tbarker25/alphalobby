#pragma once

struct Option;
void CreateLoginBox(void);
void CreateHostBattleDlg(void);
void CreateChangePasswordDlg(void);
void CreatePreferencesDlg(void);
struct User;
union UserOrBot;

LPARAM GetTextDlg2(HWND window, const char *title, char *buff, size_t buffLen);
LPARAM GetTextDlg(const char *title, char *buff, size_t buffLen);

typedef struct _iobuf FILE;
void CreateAgreementDlg(FILE *text);
void CreateColorDlg(union UserOrBot *);
void CreateSinglePlayerDlg(void);
void CreateAboutDlg(void);
void CreateReplayDlg(void);
void CreateRapidDlg(void);
