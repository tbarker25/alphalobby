#include <inttypes.h>

#include <windows.h>

HFONT gFont;
uint16_t baseUnitX;
uint16_t baseUnitY;
uint32_t scrollWidth;

void InitializeSystemMetrics(void)
{
	NONCLIENTMETRICS info = {.cbSize = sizeof(NONCLIENTMETRICS)};
	
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &info, 0);
	scrollWidth = info.iScrollWidth;
	gFont = CreateFontIndirect(&info.lfMessageFont);
	
	HDC dc = GetDC(NULL);
	SelectObject(dc, (HGDIOBJ)(HFONT)gFont);
	TEXTMETRIC tmp;
	GetTextMetrics(dc, &tmp);
	ReleaseDC(NULL, dc);
	baseUnitX = tmp.tmAveCharWidth;
	baseUnitY = tmp.tmHeight;
}

