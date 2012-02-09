#pragma once
#define WC_DOWNLOADTAB L"DownloadTab"

extern HWND gDownloadTabWindow;
void RemoveDownload(const wchar_t *name);
void UpdateDownload(const wchar_t *name, const wchar_t *text);
