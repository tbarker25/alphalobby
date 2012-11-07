#pragma once

void Downloader_Init(void);

enum DLTYPE {
	DLTYPE_MOD,
	DLTYPE_MAP,
	DLTYPE_SHORTMOD,
};

void GetSelectedPackages(void);
void DownloadFile(const char *name, enum DLTYPE type);
#define DownloadMap(mapName)\
	DownloadFile(mapName, DLTYPE_MAP)
#define DownloadMod(modName)\
	DownloadFile(modName, DLTYPE_MOD)
#define DownloadShortMod(modName)\
	DownloadFile(modName, DLTYPE_SHORTMOD)

