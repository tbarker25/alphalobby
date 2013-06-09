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

#ifndef DOWNLOADER_H
#define DOWNLOADER_H

enum DLTYPE {
	DLTYPE_MOD,
	DLTYPE_MAP,
	DLTYPE_SHORTMOD,
};

void Downloader_init(void);
void Downloader_get_selected_packages(void);
void Downloader_get_file(const char *name, enum DLTYPE type);
#define DownloadMap(map_name)\
	Downloader_get_file(map_name, DLTYPE_MAP)
#define DownloadMod(mod_name)\
	Downloader_get_file(mod_name, DLTYPE_MOD)
#define DownloadShortMod(mod_name)\
	Downloader_get_file(mod_name, DLTYPE_SHORTMOD)

#endif /* end of include guard: DOWNLOADER_H */
