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

#ifndef DIALOGBOXES_H
#define DIALOGBOXES_H

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

#endif /* end of include guard: DIALOGBOXES_H */
