#pragma once

struct SessionContext;

extern int nbDownloads;

void DownloadFile(const char *name, int isMap);
#define DownloadMap(mapName)\
	DownloadFile(mapName, 1)
#define DownloadMod(modName)\
	DownloadFile(modName, 0)
	
void ForEachDownload(void (*func)(HWND progressBar, HWND button, const wchar_t *text));
void EndDownload(struct SessionContext *ses);

