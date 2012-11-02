#include <stdio.h>
#include <inttypes.h>
#include <windows.h>
#include <windowsx.h>
#include <oleacc.h>
#include <Commctrl.h>
#include "wincommon.h"

#include <Commctrl.h>
#include "data.h"
#include "imagelist.h"
#include "icons.h"
#include "resource.h"

HIMAGELIST gIconList;

int GetColorIndex(const union UserOrBot *s)
{
	if (!(s->battleStatus & MODE_MASK))
		return -1;
	
	static const UserOrBot *colors[ICONS_LAST_COLOR - ICONS_FIRST_COLOR + 1];
	int i;
	for (i=0; i <= ICONS_LAST_COLOR - ICONS_FIRST_COLOR; ++i)
		if (colors[i] == s)
			goto doit;

	for (i=0; i <= ICONS_LAST_COLOR - ICONS_FIRST_COLOR; ++i) {
		FOR_EACH_PLAYER(p, gMyBattle)
			if (p == colors[i])
				goto end;
		doit:
		colors[i] = s;
		HDC dc = GetDC(NULL);
		HDC bitmapDC = CreateCompatibleDC(dc);
		HBITMAP bitmap = CreateCompatibleBitmap(dc, 16, 16);
		ReleaseDC(NULL, dc);
		SelectObject(bitmapDC, bitmap);
		for (int i=0; i<16*16; ++i)
			SetPixelV(bitmapDC, i%16, i/16, s->color);
		DeleteDC(bitmapDC);
		
		ImageList_Replace(gIconList, ICONS_FIRST_COLOR + i, bitmap, NULL);
		DeleteObject(bitmap);
		return ICONS_FIRST_COLOR + i;

		end:;
	}
	return -1;
}

#define ICONS_BBP (4)
#define ICONS_HEIGHT (16)
#define ICONS_WIDTH (sizeof(iconData) / ICONS_BBP / ICONS_HEIGHT)

static void __attribute__((constructor)) Init(void)
{
	gIconList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, ICONS_LAST+1);

	HBITMAP iconsBitmap = CreateBitmap(ICONS_WIDTH, ICONS_HEIGHT, 1, ICONS_BBP*8, iconData);
	ImageList_Add(gIconList, iconsBitmap, NULL);
	DeleteObject(iconsBitmap);
	
	for (int i=1; i<=ICONS_MASK; ++i)
		if (i & ~ USER_MASK)
			ImageList_SetOverlayImage(gIconList, i, i);
}
