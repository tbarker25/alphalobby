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
struct User;
union  UserOrBot;
typedef struct _iobuf FILE;

void   CreateAboutDlg(void);
void   CreateAgreementDlg(FILE *text);
void   CreateChangePasswordDlg(void);
void   CreateColorDlg(union UserOrBot *);
void   CreateHostBattleDlg(void);
void   CreateLoginDlg(void);
void   CreatePreferencesDlg(void);
void   CreateRapidDlg(void);
void   CreateReplayDlg(void);
void   CreateSinglePlayerDlg(void);
LPARAM GetTextDlg(const char *title, char *buf, size_t buf_len);
LPARAM GetTextDlg2(HWND window, const char *title, char *buf, size_t buf_len);

#endif /* end of include guard: DIALOGBOXES_H */
