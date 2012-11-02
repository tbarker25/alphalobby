#pragma once

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

