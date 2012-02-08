#pragma once

struct SessionContext;

extern int nbDownloads;

void DownloadFile(const char *name, int isMap);
#define DownloadMap(mapName)\
	DownloadFile(mapName, 1)
#define DownloadMod(modName)\
	DownloadFile(modName, 0)
	

