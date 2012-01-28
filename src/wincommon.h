#pragma once

#ifndef _WIN32_IE
	#define _WIN32_IE 0x0600
#endif
#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0600
#endif
#ifndef WINVER
	#define WINVER 0x0600
#endif

#include <windows.h>
#include <Windowsx.h>
#include <oleacc.h>
#include <Commctrl.h>
#include <stdio.h>

#include "common.h"

typedef struct DialogItem {
	wchar_t *class, *name;
	DWORD style, exStyle;
	// uint8_t flags;
} DialogItem;


typedef enum ChatDest {
	DEST_BATTLE,
	DEST_PRIVATE,
	DEST_CHANNEL,
	DEST_SERVER,
	DEST_LAST = DEST_SERVER,
}ChatDest;


HWND CreateDlgItem(HWND parent, const DialogItem *item, int dlgID);
void CreateDlgItems(HWND parent, const DialogItem items[], size_t n);

#define CreateWindowExW MyCreateWindowExW
HWND MyCreateWindowExW(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle,
		int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
		
		
int MyGetWindowTextA(HWND, char *str, int strLen);
#define GetWindowTextA MyGetWindowTextA
#define GetDlgItemTextA(_win, _id, _str, _len) MyGetWindowTextA(GetDlgItem((_win), (_id)), (_str), (_len))

//returns statically allocated memory (so only use in main thread, valid only until next call to MyGetWindowTextA2)
char * MyGetWindowTextA2(HWND window);
#define GetWindowTextA2 MyGetWindowTextA2
#define GetDlgItemTextA2(_win, _id) MyGetWindowTextA2(GetDlgItem((_win), (_id)))


wchar_t *utf8to16(const char *str);
char *utf16to8(const wchar_t *wStr);
//Const before 'file' starts, ie appending is ok. Valid until next call:
const wchar_t * GetWritablePath_unsafe(wchar_t *file);
void GetWritablePath(wchar_t *file, wchar_t *buff);
extern wchar_t gWritableDataDirectory[MAX_PATH];
extern size_t gWritableDataDirectoryLen;

#define SS_REALSIZECONTROL      0x40
#define PBS_MARQUEE             0x08
#define PBM_SETRANGE            (WM_USER+1)
#define PBM_SETPOS              (WM_USER+2)
#define PBM_DELTAPOS            (WM_USER+3)
#define PBM_SETSTEP             (WM_USER+4)
#define PBM_STEPIT              (WM_USER+5)
#define PBM_SETBKCOLOR          CCM_SETBKCOLOR
#define PBS_MARQUEE             0x08
#define PBM_SETMARQUEE          (WM_USER+10)
#define PBS_SMOOTHREVERSE       0x10
#define PBM_GETSTEP             (WM_USER+13)
#define PBM_GETBKCOLOR          (WM_USER+14)
#define PBM_GETBARCOLOR         (WM_USER+15)
#define PBM_SETSTATE            (WM_USER+16)
#define PBM_GETSTATE            (WM_USER+17)
#define PBST_NORMAL             0x0001
#define PBST_ERROR              0x0002
#define PBST_PAUSED             0x0003

#define splitterWidth (2*RELATED_SPACING_X)
#define bottomMargin (2 * RELATED_SPACING_Y + COMMANDBUTTON_Y)
#define LVM_MAPINDEXTOID     (LVM_FIRST + 180)

#define CFE_LINK		0x0020
#define CFM_LINK		0x00000020
#define CFM_STYLE			0x00080000
#define CFM_KERNING			0x00100000

typedef struct LVGROUP {
  UINT   cbSize;
  UINT   mask;
  LPWSTR pszHeader;
  int    cchHeader;
  LPWSTR pszFooter;
  int    cchFooter;
  int    iGroupId;
  UINT   stateMask;
  UINT   state;
  UINT   uAlign;
#if _WIN32_WINNT >= 0x0600
  LPWSTR pszSubtitle;
  UINT   cchSubtitle;
  LPWSTR pszTask;
  UINT   cchTask;
  LPWSTR pszDescriptionTop;
  UINT   cchDescriptionTop;
  LPWSTR pszDescriptionBottom;
  UINT   cchDescriptionBottom;
  int    iTitleImage;
  int    iExtendedImage;
  int    iFirstItem;
  UINT   cItems;
  LPWSTR pszSubsetTitle;
  UINT   cchSubsetTitle;
#endif 
} LVGROUP, *PLVGROUP;

#define LVGF_NONE           0x00000000
#define LVGF_HEADER         0x00000001
#define LVGF_FOOTER         0x00000002
#define LVGF_STATE          0x00000004
#define LVGF_ALIGN          0x00000008
#define LVGF_GROUPID        0x00000010
#if _WIN32_WINNT >= 0x0600
#define LVGF_SUBTITLE           0x00000100  // pszSubtitle is valid
#define LVGF_TASK               0x00000200  // pszTask is valid
#define LVGF_DESCRIPTIONTOP     0x00000400  // pszDescriptionTop is valid
#define LVGF_DESCRIPTIONBOTTOM  0x00000800  // pszDescriptionBottom is valid
#define LVGF_TITLEIMAGE         0x00001000  // iTitleImage is valid
#define LVGF_EXTENDEDIMAGE      0x00002000  // iExtendedImage is valid
#define LVGF_ITEMS              0x00004000  // iFirstItem and cItems are valid
#define LVGF_SUBSET             0x00008000  // pszSubsetTitle is valid
#define LVGF_SUBSETITEMS        0x00010000  // readonly, cItems holds count of items in visible subset, iFirstItem is valid
#endif

#define LVGS_NORMAL             0x00000000
#define LVGS_COLLAPSED          0x00000001
#define LVGS_HIDDEN             0x00000002
#define LVGS_NOHEADER           0x00000004
#define LVGS_COLLAPSIBLE        0x00000008
#define LVGS_FOCUSED            0x00000010
#define LVGS_SELECTED           0x00000020
#define LVGS_SUBSETED           0x00000040
#define LVGS_SUBSETLINKFOCUSED  0x00000080

#define LVGA_HEADER_LEFT    0x00000001
#define LVGA_HEADER_CENTER  0x00000002
#define LVGA_HEADER_RIGHT   0x00000004  // Don't forget to validate exclusivity
#define LVGA_FOOTER_LEFT    0x00000008
#define LVGA_FOOTER_CENTER  0x00000010
#define LVGA_FOOTER_RIGHT   0x00000020  // Don't forget to validate exclusivity

#define LVS_EX_DOUBLEBUFFER 0x00010000

#define LVN_HOTTRACK            (LVN_FIRST-21)

typedef struct tagLVSETINFOTIP
{
    UINT cbSize;
    DWORD dwFlags;
    LPWSTR pszText;
    int iItem;
    int iSubItem;
} LVSETINFOTIP, *PLVSETINFOTIP;
