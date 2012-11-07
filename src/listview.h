#pragma once
#define LVM_MAPINDEXTOID     (LVM_FIRST + 180)

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



