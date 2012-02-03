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



// ====================== Button Control =============================

#ifndef NOBUTTON

#ifdef _WIN32

// Button Class Name
#define WC_BUTTONA              "Button"
#define WC_BUTTONW              L"Button"

#ifdef UNICODE
#define WC_BUTTON               WC_BUTTONW
#else
#define WC_BUTTON               WC_BUTTONA
#endif

#else
#define WC_BUTTON               "Button"
#endif

#if (_WIN32_WINNT >= 0x0501)
#define BUTTON_IMAGELIST_ALIGN_LEFT     0
#define BUTTON_IMAGELIST_ALIGN_RIGHT    1
#define BUTTON_IMAGELIST_ALIGN_TOP      2
#define BUTTON_IMAGELIST_ALIGN_BOTTOM   3
#define BUTTON_IMAGELIST_ALIGN_CENTER   4       // Doesn't draw text

typedef struct
{
    HIMAGELIST  himl;   // Images: Normal, Hot, Pushed, Disabled. If count is less than 4, we use index 1
    RECT        margin; // Margin around icon.
    UINT        uAlign;
} BUTTON_IMAGELIST, *PBUTTON_IMAGELIST;

#define BCM_GETIDEALSIZE        (BCM_FIRST + 0x0001)
#define Button_GetIdealSize(hwnd, psize)\
    (BOOL)SNDMSG((hwnd), BCM_GETIDEALSIZE, 0, (LPARAM)(psize))

#define BCM_SETIMAGELIST        (BCM_FIRST + 0x0002)
#define Button_SetImageList(hwnd, pbuttonImagelist)\
    (BOOL)SNDMSG((hwnd), BCM_SETIMAGELIST, 0, (LPARAM)(pbuttonImagelist))

#define BCM_GETIMAGELIST        (BCM_FIRST + 0x0003)
#define Button_GetImageList(hwnd, pbuttonImagelist)\
    (BOOL)SNDMSG((hwnd), BCM_GETIMAGELIST, 0, (LPARAM)(pbuttonImagelist))

#define BCM_SETTEXTMARGIN       (BCM_FIRST + 0x0004)
#define Button_SetTextMargin(hwnd, pmargin)\
    (BOOL)SNDMSG((hwnd), BCM_SETTEXTMARGIN, 0, (LPARAM)(pmargin))
#define BCM_GETTEXTMARGIN       (BCM_FIRST + 0x0005)
#define Button_GetTextMargin(hwnd, pmargin)\
    (BOOL)SNDMSG((hwnd), BCM_GETTEXTMARGIN, 0, (LPARAM)(pmargin))

typedef struct tagNMBCHOTITEM
{
    NMHDR   hdr;
    DWORD   dwFlags;           // HICF_*
} NMBCHOTITEM, * LPNMBCHOTITEM;

#define BCN_HOTITEMCHANGE       (BCN_FIRST + 0x0001)

#define BST_HOT            0x0200

#endif // _WIN32_WINNT >= 0x0501

#if _WIN32_WINNT >= 0x0600

// BUTTON STATE FLAGS
#define BST_DROPDOWNPUSHED      0x0400

// begin_r_commctrl

// BUTTON STYLES
#define BS_SPLITBUTTON          0x0000000CL
#define BS_DEFSPLITBUTTON       0x0000000DL
#define BS_COMMANDLINK          0x0000000EL
#define BS_DEFCOMMANDLINK       0x0000000FL

// SPLIT BUTTON INFO mask flags
#define BCSIF_GLYPH             0x0001
#define BCSIF_IMAGE             0x0002
#define BCSIF_STYLE             0x0004
#define BCSIF_SIZE              0x0008

// SPLIT BUTTON STYLE flags
#define BCSS_NOSPLIT            0x0001
#define BCSS_STRETCH            0x0002
#define BCSS_ALIGNLEFT          0x0004
#define BCSS_IMAGE              0x0008

// end_r_commctrl

// BUTTON STRUCTURES
typedef struct tagBUTTON_SPLITINFO
{
    UINT        mask;
    HIMAGELIST  himlGlyph;         // interpreted as WCHAR if BCSIF_GLYPH is set
    UINT        uSplitStyle;
    SIZE        size;
} BUTTON_SPLITINFO, * PBUTTON_SPLITINFO;

// BUTTON MESSAGES
#define BCN_FIRST               (0U-1250U)
#define BCM_SETDROPDOWNSTATE     (BCM_FIRST + 0x0006)
#define Button_SetDropDownState(hwnd, fDropDown) \
    (BOOL)SNDMSG((hwnd), BCM_SETDROPDOWNSTATE, (WPARAM)(fDropDown), 0)

#define BCM_SETSPLITINFO         (BCM_FIRST + 0x0007)
#define Button_SetSplitInfo(hwnd, pInfo) \
    (BOOL)SNDMSG((hwnd), BCM_SETSPLITINFO, 0, (LPARAM)(pInfo))

#define BCM_GETSPLITINFO         (BCM_FIRST + 0x0008)
#define Button_GetSplitInfo(hwnd, pInfo) \
    (BOOL)SNDMSG((hwnd), BCM_GETSPLITINFO, 0, (LPARAM)(pInfo))

#define BCM_SETNOTE              (BCM_FIRST + 0x0009)
#define Button_SetNote(hwnd, psz) \
    (BOOL)SNDMSG((hwnd), BCM_SETNOTE, 0, (LPARAM)(psz))

#define BCM_GETNOTE              (BCM_FIRST + 0x000A)
#define Button_GetNote(hwnd, psz, pcc) \
    (BOOL)SNDMSG((hwnd), BCM_GETNOTE, (WPARAM)pcc, (LPARAM)psz)

#define BCM_GETNOTELENGTH        (BCM_FIRST + 0x000B)
#define Button_GetNoteLength(hwnd) \
    (LRESULT)SNDMSG((hwnd), BCM_GETNOTELENGTH, 0, 0)


#if _WIN32_WINNT >= 0x0600
// Macro to use on a button or command link to display an elevated icon
#define BCM_SETSHIELD            (BCM_FIRST + 0x000C)
#define Button_SetElevationRequiredState(hwnd, fRequired) \
    (LRESULT)SNDMSG((hwnd), BCM_SETSHIELD, 0, (LPARAM)fRequired)
#endif /* _WIN32_WINNT >= 0x0600 */

// Value to pass to BCM_SETIMAGELIST to indicate that no glyph should be
// displayed
#define BCCL_NOGLYPH  (HIMAGELIST)(-1)

// NOTIFICATION MESSAGES
typedef struct tagNMBCDROPDOWN
{
    NMHDR   hdr;
    RECT    rcButton;
} NMBCDROPDOWN, * LPNMBCDROPDOWN;

#define BCN_DROPDOWN            (BCN_FIRST + 0x0002)

#endif // _WIN32_WINNT >= 0x600

#endif // NOBUTTON

