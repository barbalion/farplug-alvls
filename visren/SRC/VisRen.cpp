/****************************************************************************
 * VisRen.cpp
 *
 * Plugin module for FAR Manager 1.75
 *
 * Copyright (c) 2007-2010 Alexey Samlyukov
 ****************************************************************************/

/* $ Revision: 10.0 $ */

#define _FAR_NO_NAMELESS_UNIONS
#define _FAR_USE_FARFINDDATA

#include <tchar.h>
#include "..\..\plugin.hpp"
#include "..\..\farkeys.hpp"
#include "..\..\farcolor.hpp"
#include "PCRE77\pcre.h"
#include "VisRen1_LNG.cpp"        // ����� �������� ��� ���������� ����� �� .lng �����

/// �����! ���������� ������ �������, ���� ������������� �� �������� ������
#define my_malloc(size) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size)
#define my_free(ptr) ((ptr)?HeapFree(GetProcessHeap(),0,ptr):0)
#define my_realloc(ptr,size) ((size)?((ptr)?HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,ptr,size): \
        HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size)):(HeapFree(GetProcessHeap(),0,ptr),(void *)0))

/****************************************************************************
 * ����� ����������� �������� FAR
 ****************************************************************************/
static struct PluginStartupInfo Info;
static struct FarStandardFunctions FSF;
static TCHAR  PluginRootKey[NM];
struct InitDialogItem {
  int      Type;
  int      X1, Y1, X2, Y2;
  int      Focus;
  int      Selected;
  unsigned Flags;
  int      DefaultButton;
  TCHAR   *Data;
};

/****************************************************************************
 * ������� ��������� �������
 ****************************************************************************/
struct Options {
  TCHAR MaskName[512],
        MaskExt[512],
        Search[512],
        Replace[512],
        WordDiv[80];
  int UseLastHistory,
      lenFileName,
      lenName,
      lenExt,
      lenDestFileName,
      lenDestName,
      lenDestExt,
      CurBorder,
      srcCurCol,
      destCurCol,
      ShowOrgName,
      CaseSensitive,
      RegEx,
      LogRen,                             // �������� �� ���������� ������
      LoadUndo,                           // ���� ���� ����� - ��������/���������
      Undo;                               // ���� ���� ����� - �������� ���
} Opt;

/****************************************************************************
 * ����� ����������
 ****************************************************************************/
int StartPosX,
    SaveItemFocus,
    Focus;

bool bStartSelect,
     bError;

struct FileIndex {                               // �������� ��� ��������������
  PluginPanelItem *ppi;                          //   ����� ������������ ����� � ������
  TCHAR **DestFileName;                          //   �������� �����
  int  iCount;                                   //   ���-��
} sFI;

struct UndoFileIndex {                           // �������� ��� ������ ��������������
  TCHAR *Dir;                                    //   ����� � ������� ���������������
  TCHAR **CurFileName;                           //   �����, � ������� ������������� �����
  TCHAR **OldFileName;                           //   �����, ������� ���� � ������ �� ��������������
  int  iCount;                                   //   ���-��
} sUndoFI;

/****************************************************************************
 * ������ ��������� ������� FAR: ��������� ������ �� .lng-�����
 ****************************************************************************/
static const TCHAR *GetMsg(int MsgId)
{
  return Info.GetMsg(Info.ModuleNumber, MsgId);
}

/****************************************************************************
 * ����� ��������������-������ � ���������� � ����� ��������
 ****************************************************************************/
static void ErrorMsg(DWORD Title, DWORD Body)
{
  const TCHAR *MsgItems[]={ GetMsg(Title), GetMsg(Body), GetMsg(MOK) };
  Info.Message(Info.ModuleNumber, FMSG_WARNING, 0, MsgItems, 3, 1);
}

/****************************************************************************
 * ����� �������������� "Yes-No" � ���������� � ����� ��������
 ****************************************************************************/
static bool YesNoMsg(DWORD Title, DWORD Body)
{
  const TCHAR *MsgItems[]={ GetMsg(Title), GetMsg(Body) };
  return (!Info.Message(Info.ModuleNumber, FMSG_WARNING|FMSG_MB_YESNO, 0, MsgItems, 2, 0));
}

// ��������� ��� �������
static int DebugMsg(TCHAR *msg, TCHAR *msg2 = _T(" "), unsigned int i = 1000)
{
  TCHAR *MsgItems[] = {_T("DebugMsg"), _T(""), _T(""), _T("")};
  TCHAR buf[80]; FSF.itoa(i, buf,10);
  MsgItems[1] = msg2;
  MsgItems[2] = msg;
  MsgItems[3] = buf;
  return (!Info.Message(Info.ModuleNumber, FMSG_WARNING|FMSG_MB_OKCANCEL, 0,
                        MsgItems, sizeof(MsgItems) / sizeof(MsgItems[0]),2));
}


#include "VisRen2_REG.cpp"        // �-��� ��� ������ � ��������
#include "VisRen3_MP3.cpp"        // �-��� ��� �������������� �� ����� MP3
#include "VisRen4_JPG.cpp"        // �-��� ��� ������ � JPG
#include "VisRen5_REN.cpp"        // �-��� ��� �������������� � ������
#include "VisRen6_DLG.cpp"        // �-��� ��� ��������� ������� �������
#include "VisRen7_EDT.cpp"        // �-��� ��� �������������� � ���������

/****************************************************************************
 ***************************** Exported functions ***************************
 ****************************************************************************/

static bool bOldFAR=false, bWin9x=false;

/****************************************************************************
 * ��� ������� ������� FAR �������� � ������ �������
 ****************************************************************************/
// ��������� ���������� �������������� ������ FAR�...
int WINAPI _export GetMinFarVersion() { return MAKEFARVERSION(1,75,2616); }

// �������� ��������� PluginStartupInfo � ������� ��� �������� ��������...
void WINAPI _export SetStartupInfo(const struct PluginStartupInfo *Info)
{
  ::Info = *Info;
  if (Info->StructSize >= (int)sizeof(struct PluginStartupInfo))
  {
    FSF = *Info->FSF;
    ::Info.FSF = &FSF;
    Opt.UseLastHistory=0;
    // ��������� ���� �������
    FSF.sprintf(PluginRootKey, _T("%s\\VisRen"), Info->RootKey);
    // ������� ��������� ��� �������� ������
    memset(&sUndoFI, 0, sizeof(sUndoFI));

    OSVERSIONINFO ovi;
    ovi.dwOSVersionInfoSize=sizeof(ovi);
    if (GetVersionEx(&ovi) && ovi.dwPlatformId!=VER_PLATFORM_WIN32_NT)
      bWin9x=true;
    // �������� � ���������������� ��������!
    DlgSize.Full=false;
  }
  else
    bOldFAR = true;
}

/****************************************************************************
 * ��� ������� ������� FAR �������� �� ������ ������� - �������� PluginInfo, �.�.
 * ������ FAR� ����� ������ �������� � "Plugin commands" � "Plugins configuration".
 ****************************************************************************/
void WINAPI _export GetPluginInfo(struct PluginInfo *Info)
{
  static const TCHAR *PluginMenuStrings[1];
  PluginMenuStrings[0]            = GetMsg(MVRenTitle);

  Info->StructSize                = (int)sizeof(*Info);
  Info->PluginMenuStrings         = PluginMenuStrings;
  Info->PluginMenuStringsNumber   = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
}

/****************************************************************************
 * �������� ������� �������. FAR � ��������, ����� ������������ ���� ������
 ****************************************************************************/
HANDLE WINAPI _export OpenPlugin(int OpenFrom, INT_PTR Item)
{
  HANDLE hPlugin = INVALID_HANDLE_VALUE;

  // ���� ������ ���� ������� �����...
  if (bOldFAR)
  {
    ErrorMsg(MOldFARTitle, MOldFARBody);
    return hPlugin;
  }

  struct PanelInfo PInfo;

  // ���� �� ������� ��������� ���������� � ������...
  if (!Info.Control(INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &PInfo))
  {
    return hPlugin;
  }

  if ( PInfo.PanelType != PTYPE_FILEPANEL
       || PInfo.Plugin /*&& !(PInfo.Flags & PFLAGS_REALNAMES))*/
       || !PInfo.ItemsNumber
       || !PInfo.SelectedItemsNumber )
  {
    ErrorMsg(MVRenTitle, MFilePanelsRequired);
    return hPlugin;
  }

  // �������� ������ ��� ������ �������� � �������� ���� ������
  if (  !(sFI.ppi=(PluginPanelItem*)my_malloc(PInfo.SelectedItemsNumber * sizeof(PluginPanelItem)))
     || !(sFI.DestFileName=(TCHAR**)my_malloc(PInfo.SelectedItemsNumber * sizeof(TCHAR*)))  )
  {
    ErrorMsg(MVRenTitle, MErrorNoMem); return hPlugin;
  }

  bool Ret=true;
  // �������������� ������
  memcpy(sFI.ppi, PInfo.SelectedItems, (sFI.iCount=PInfo.SelectedItemsNumber) * sizeof(*sFI.ppi));
  for (int i=0; i<sFI.iCount; i++)
  {
    sFI.DestFileName[i]=(TCHAR*)my_malloc(MAX_PATH*sizeof(TCHAR));
    if (!sFI.DestFileName[i]) { Ret=false; break; }
    lstrcpy(sFI.DestFileName[i], sFI.ppi[i].FindData.cFileName);
  }

  if (!Ret)
    ErrorMsg(MVRenTitle, MErrorNoMem);
  else
  {
    switch (ShowDialog())
    {
      case 0:
        RenameFile(&PInfo); break;
      case 2:
        RenameInEditor(&PInfo); break;
    }
  }

  //��������� ������
  for (int i=0; i<sFI.iCount; i++)
    my_free(sFI.DestFileName[i]);
  my_free(sFI.DestFileName); sFI.DestFileName=0;
  my_free(sFI.ppi); sFI.ppi=0;

  return hPlugin;
}

/****************************************************************************
 * ��� ������� FAR �������� ����� ��������� �������
 ****************************************************************************/
void WINAPI _export ExitFAR(void)
{
  //��������� ������ � ������ �������� �������
  FreeUndo();
}
