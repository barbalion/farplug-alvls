/****************************************************************************
 * compare.cpp
 *
 * Plugin module for FAR Manager 1.71
 *
 * Copyright (c) 1996-2000 Eugene Roshal
 * Copyright (c) 2000-2007 FAR group
 * Copyright (c) 2006-2007 Alexey Samlyukov
 ****************************************************************************/

/* Current developer: samlyukov<at>gmail.com  */

/* $ Revision: 030.4 $ */

#define _FAR_NO_NAMELESS_UNIONS
#define _FAR_USE_FARFINDDATA

#ifdef __GNUC__
 #define _i64(num) num##ll
 #define _ui64(num) num##ull
#else
 #define _i64(num) num##i64
 #define _ui64(num) num##ui64
#endif

#include "..\..\plugin.hpp"
#include "..\..\farkeys.hpp"
#include "..\..\farcolor.hpp"

#include "compare1_LNG.cpp"        // ����� �������� ��� ���������� ����� �� .lng �����

/****************************************************************************
 * ����� ��� ���������� ��������� startup-���� ��� ���������� ��� GCC
 ****************************************************************************/
#if defined(__GNUC__)
#include "crt.hpp"

#ifdef __cplusplus
extern "C"{
#endif
  BOOL WINAPI DllMainCRTStartup(HANDLE hDll, DWORD dwReason, LPVOID lpReserved);
#ifdef __cplusplus
};
#endif

BOOL WINAPI DllMainCRTStartup(HANDLE hDll, DWORD dwReason, LPVOID lpReserved)
{
  (void) lpReserved;
  (void) dwReason;
  (void) hDll;
  return true;
}
#endif


/****************************************************************************
 * ����� ����������� �������� FAR
 ****************************************************************************/
static struct PluginStartupInfo Info;
static struct FarStandardFunctions FSF;
static char   PluginRootKey[NM];

static void WFD2FFD(WIN32_FIND_DATA &wfd, FAR_FIND_DATA &ffd)
{
  ffd.dwFileAttributes = wfd.dwFileAttributes;
  ffd.ftCreationTime   = wfd.ftCreationTime;
  ffd.ftLastAccessTime = wfd.ftLastAccessTime;
  ffd.ftLastWriteTime  = wfd.ftLastWriteTime;
  ffd.nFileSizeHigh    = wfd.nFileSizeHigh;
  ffd.nFileSizeLow     = wfd.nFileSizeLow;
  ffd.dwReserved0      = wfd.dwReserved0;
  ffd.dwReserved1      = wfd.dwReserved1;

  lstrcpy(ffd.cFileName, wfd.cFileName);
  lstrcpy(ffd.cAlternateFileName, wfd.cAlternateFileName);
}

/****************************************************************************
 * ������� ��������� �������
 ****************************************************************************/
struct Options {
 char MasksInclude[NM],
      MasksExclude[NM],
      FilterPattern[NM*5],
      PathDiffProgram[MAX_PATH];
  int CompareName,
      IgnoreCase,
      CompareSize,
      CompareTime,
      LowPrecisionTime,
      IgnoreTimeZone,
      CompareContents,
      ContentsPercent,
      Filter,
      FilterTemplatesN,
      ProcessSubfolders,
      MaxScanDepth,
      ProcessSelected,
      ProcessTillFirstDiff,
      Panel,
      StartupWithAsterisks,
      Cache,
      ShowMessage,
      Sound,
      UseBufSize,
      BufSize,
      RunDiffProgram,
      ProcessHidden,
      UseCacheResult;
} Opt;

/****************************************************************************
 * ����� ����������
 ****************************************************************************/
static HANDLE hConInp = INVALID_HANDLE_VALUE;                     // ����� ������. �����
static bool bPlatformNT,                                          // ��������� NT?
            bBrokenByEsc,                                         // ����������?
            bAPluginPanels, bPPluginPanels,                       // ������ �������?
            bAPanelWithCRC, bPPanelWithCRC,                       //    ... ������?
            bAPanelWithDir, bPPanelWithDir,                       //    ... � ����������?
            bAPanelTmp, bPPanelTmp,                               //    ... ���� ������?
            bStartMsg,                                            // ����� ���������?
            bBuildCacheResult,                                    // ����� ���������� ����������?
            bRemovableDrive;                                      // ���� ���� � ����� �: ?
static int iTruncLen;                                             // ������������ ����� ���������
static char *ABuf=0, *PBuf=0;                                     // ����� ������

struct InitDialogItem {                                           // ������� �������
  int      Type;
  int      X1, Y1, X2, Y2;
  int      Focus;
  int      Selected;
  unsigned Flags;
  int      DefaultButton;
  char    *Data;
};

struct FileIndex {                                                // �������� ��� ���������
  PluginPanelItem **ppi;                                          //    ���� ��������
  int iCount;                                                     //    ���-��
};

struct TotalDiffItems {                                           // ����� ���-�� ��������� ���������-��������
  DWORD    nDiffNo;                                               //    ... ����������
  DWORD    nDiffA;                                                //    ... ���������� �� ��������
  DWORD    nDiffP;                                                //    ... ���������� �� ���������
} Diff;

struct InDirItems {                                               // ���-�� ��������� � ������
  DWORD    nCount;                                                //    ����������
  DWORD    nProcess;                                              //    ��� ����������
  DWORD64  n2Size;                                                //    ������ ������������ ���� ������
  DWORD64  n2SizeProcess;                                         //    ������ ��� ������������ �����
} Work;

struct ResultCmpItem {                                            // ��������� ��������� 2-� ���������
  DWORD    dwFileName[2];                                         //  (��� ��������� ����������
  DWORD64  dwWriteTime[2];                                        //          ���������)
  DWORD    dwFlags;
};

enum dwFlagsEnable {                                              // ResultCmpItem.dwFlags
  Flag1 = 257
};

enum dwFlagsDisable {                                             // ResultCmpItem.dwFlags
  Flag0 = 256
};

//#define CACHE
#ifdef CACHE
struct ResultCmpDir {                                             // ���������� ��������� ������� �����
  DWORD    dwCurDir[2];                                           //    ����� ������� ����� (������ � � P)
  ResultCmpItem *RCI;                                             //    ������ ��������� � ������
  int iCount;                                                     //    ���������� � �������
};

struct ResultCache {                                              // ��� ���������
  ResultCmpDir *RCD;                                              //    ������ �����
  int iCount;                                                     //    ���������� � �������
} Cache;
#else
struct ResultCmpContents {                                        // ���������� ��������� "�� �����������"
  ResultCmpItem *rci;
  int iCount;
} CacheResult;
#endif

struct ParamStore {                                               // �������-�������� ��� ���������� � ������
  int      ID;
  char    *RegName;
  int     *StoreToOpt;
};

class CmpPanel                                                    // ����� ������
{
  PluginPanelItem *CmpPanelItem;
  int CmpItemsNumber;
  void FreePanelItem();
public:
  CmpPanel();
  ~CmpPanel();
  void GetOpenPluginInfo(OpenPluginInfo*);
  int GetFindData(PluginPanelItem**, int*, int);
  void BuildItem( const char*, const char*,
       const FAR_FIND_DATA*, const FAR_FIND_DATA* );
  int CheckItem();
};

CmpPanel *Panel = 0;                                              // ������


/****************************************************************************
 * ������ ��������� ������� FAR: ��������� ������ �� .lng-�����
 ****************************************************************************/
static const char *GetMsg(int CompareLng)
{
  return Info.GetMsg(Info.ModuleNumber, CompareLng);
}

/****************************************************************************
 * ����� ��������������-������ � ���������� � ����� ��������
 ****************************************************************************/
static void ErrorMsg(DWORD Title, DWORD Body)
{
  if (Opt.Sound) MessageBeep(MB_ICONHAND);
  const char *MsgItems[]={ GetMsg(Title), GetMsg(Body), GetMsg(MOK) };
  Info.Message(Info.ModuleNumber, FMSG_WARNING, 0, MsgItems, 3, 1);
}

/****************************************************************************
 * ����� �������������� "Yes-No" � ���������� � ����� ��������
 ****************************************************************************/
static bool YesNoMsg(DWORD Title, DWORD Body)
{
  if (Opt.Sound) MessageBeep(MB_ICONEXCLAMATION);
  const char *MsgItems[]={ GetMsg(Title), GetMsg(Body) };
  return (!Info.Message(Info.ModuleNumber, FMSG_WARNING|FMSG_MB_YESNO, 0, MsgItems, 2, 0));
}

// ��������� ��� �������
static int DebugMsg(char *msg, char *msg2 = "debug")
{
  char *MsgItems[] = {"DebugMsg", "", ""};
  MsgItems[1] = msg2;
  MsgItems[2] = msg;
  if (!Info.Message( Info.ModuleNumber,
                     FMSG_WARNING|FMSG_MB_OKCANCEL,
                     0,
                     MsgItems,
                     sizeof(MsgItems) / sizeof(MsgItems[0]),
                     2 )) return 1;
   return 0;
}

/****************************************************************************
 * ������� ��� �������������� ������� �������� InitDialogItem � FarDialogItem.
 ****************************************************************************/
static void InitDialogItems(const InitDialogItem *Init, FarDialogItem *Item, int ItemsNumber)
{
  while(ItemsNumber--)
  {
    Item->Type           = Init->Type;
    Item->X1             = Init->X1;
    Item->Y1             = Init->Y1;
    Item->X2             = Init->X2;
    Item->Y2             = Init->Y2;
    Item->Focus          = Init->Focus;
    Item->Param.Selected = Init->Selected;
    Item->Flags          = Init->Flags;
    Item->DefaultButton  = Init->DefaultButton;
    lstrcpy( Item->Data.Data, ((unsigned int)Init->Data < 2000) ?
             GetMsg((unsigned int)Init->Data) : Init->Data );

    Item++;
    Init++;
  }
}


#include "compare2_REG.cpp"        // �-��� ��� ������ � ��������
#include "compare3_CFG.cpp"        // �-��� ��� ������� � ��������� �������� �������
#include "compare4_DLG.cpp"        // �-��� ��� ��������� ������� �������
#include "compare5_MSG.cpp"        // �-��� ��� ���������-��������� ���� ���������
#include "compare6_FLS.cpp"        // �-��� ��� ��������� ������
#include "compare7_DIR.cpp"        // �-��� ��� ��������� �����
#include "compare8_PNL.cpp"        // �-��� ��� �������� ������


/****************************************************************************
 ***************************** Exported functions ***************************
 ****************************************************************************/

static bool bOldFAR = false;

/****************************************************************************
 * ��� ������� ������� FAR �������� � ������ �������
 ****************************************************************************/
// ��������� ���������� �������������� ������ FAR�...
int WINAPI _export GetMinFarVersion() { return MAKEFARVERSION(1,71,2148); }

// �������� ��������� PluginStartupInfo � ������� ��� �������� ��������...
void WINAPI _export SetStartupInfo(const struct PluginStartupInfo *Info)
{
  ::Info = *Info;

  if (Info->StructSize >= (int)sizeof(struct PluginStartupInfo))
  {
    FSF = *Info->FSF;
    ::Info.FSF = &FSF;

    // ��������� ���� �������
    FSF.sprintf(PluginRootKey, "%s\\AdvCompare", Info->RootKey);
    // ��������� ��������� �������
    bool bShowDlg = false; Config(bShowDlg);
    // ������� ��� (���� ����� �������� ���������� ���������)
    #ifdef CACHE
    memset(&Cache, 0, sizeof(Cache));
    #else
    memset(&CacheResult, 0, sizeof(CacheResult));
    #endif
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
  static const char *PluginMenuStrings[1];
  PluginMenuStrings[0]            = GetMsg(MCompareTitle);

  Info->StructSize                = (int)sizeof(*Info);
  Info->PluginMenuStrings         = PluginMenuStrings;
  Info->PluginMenuStringsNumber   = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
  Info->PluginConfigStrings       = PluginMenuStrings;
  Info->PluginConfigStringsNumber = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
}

/****************************************************************************
 * ��� ������� FAR �������� ��� ��������� ������� �� "Plugins configuration"
 ****************************************************************************/
int WINAPI _export Configure(int ItemNumber)
{
  return(!bOldFAR && (!ItemNumber ? Config() : 0));
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

  struct PanelInfo AInfo, PInfo;

  // ���� �� ������� ��������� ���������� � �������...
  if ( !Info.Control(INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &AInfo) ||
       !Info.Control(INVALID_HANDLE_VALUE, FCTL_GETANOTHERPANELINFO, &PInfo) )
  {
    return hPlugin;
  }

  // ���� ������ ����������...
  if (AInfo.PanelType != PTYPE_FILEPANEL || PInfo.PanelType != PTYPE_FILEPANEL)
  {
    ErrorMsg(MCompareTitle, MFilePanelsRequired);
    return hPlugin;
  }

  bAPluginPanels = AInfo.Plugin; bPPluginPanels = PInfo.Plugin;
   // ���� ������� ���������� ������ � CRC32 (�����)...
  bAPanelWithCRC = bPPanelWithCRC = false;
   // ...� ������ ������� �� ����-������
  bAPanelTmp = bPPanelTmp = false;
   // ...� ������ ���� �� �� ������ ��������
  bAPanelWithDir = bPPanelWithDir = false;

  for (int i = AInfo.ItemsNumber-1; i>=0; i--)
  {
    if (!bAPanelWithCRC && bAPluginPanels && AInfo.PanelItems[i].CRC32)
      bAPanelWithCRC = true;
    if (!bAPanelTmp && (bAPluginPanels && (AInfo.Flags & PFLAGS_REALNAMES)) &&
         strpbrk(AInfo.PanelItems[i].FindData.cFileName, ":\\/") )
      bAPanelTmp = true;
    if ( !bAPanelWithDir && !bPPluginPanels &&
         ( (AInfo.PanelItems[i].FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
           lstrcmp(AInfo.PanelItems[i].FindData.cFileName, "..")  ))
      bAPanelWithDir = true;
  }

  for (int i = PInfo.ItemsNumber-1; i>=0; i--)
  {
    if (!bPPanelWithCRC && bPPluginPanels && PInfo.PanelItems[i].CRC32)
      bPPanelWithCRC = true;
    if (!bPPanelTmp && (bPPluginPanels && (PInfo.Flags & PFLAGS_REALNAMES)) &&
         strpbrk(PInfo.PanelItems[i].FindData.cFileName, ":\\/") )
      bPPanelTmp = true;
    if ( !bPPanelWithDir && !bAPluginPanels &&
         ( (PInfo.PanelItems[i].FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
           lstrcmp(PInfo.PanelItems[i].FindData.cFileName, "..") ))
      bPPanelWithDir = true;
  }

  bRemovableDrive = false;
  // ���� �� ����� �� ������� ������ ���� � ����� �:
  if (!bAPluginPanels && !bPPluginPanels)
  {
    char ARemovable[NM], PRemovable[NM];
    FSF.GetPathRoot(AInfo.CurDir, ARemovable);
    FSF.GetPathRoot(PInfo.CurDir, PRemovable);
    if (ARemovable[0] == 'A' || PRemovable[0] == 'A')
      bRemovableDrive = (GetDriveType(ARemovable)==DRIVE_REMOVABLE || GetDriveType(PRemovable)==DRIVE_REMOVABLE);
  }

  // ���� ������� �������� - �����...
  bool bCurrentItemFile = ( AInfo.ItemsNumber && PInfo.ItemsNumber
                       && !(AInfo.PanelItems[AInfo.CurrentItem].FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                       && !(PInfo.PanelItems[PInfo.CurrentItem].FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) );

  // ���� �� ����� �������� ������ �������...
  int ExitCode = ShowDialog( AInfo.Flags & PFLAGS_REALNAMES, PInfo.Flags & PFLAGS_REALNAMES, bCurrentItemFile,
                             (AInfo.SelectedItemsNumber && (AInfo.SelectedItems[0].Flags & PPIF_SELECTED)) ||
                             (PInfo.SelectedItemsNumber && (PInfo.SelectedItems[0].Flags & PPIF_SELECTED)) );
  if (ExitCode < 0)
    return hPlugin;
/*  if (ExitCode == 1)
  {
    CompareTag();
    return hPlugin;
  }
*/
  // ��������� ����������� ������ ������� ���������...
  HANDLE hConOut = CreateFile("CONOUT$", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  CONSOLE_SCREEN_BUFFER_INFO csbiNfo;
  if (GetConsoleScreenBufferInfo(hConOut, &csbiNfo))
  {
    if ((iTruncLen = csbiNfo.dwSize.X - 20) > NM - 2)
      iTruncLen = NM - 2;
    else if (iTruncLen < 0)
      iTruncLen = csbiNfo.dwSize.X - csbiNfo.dwSize.X / 4;
  }
  else
    iTruncLen = 60;
  CloseHandle(hConOut);

  // �� ����� ��������� ������� ��������� ������� ����...
  char cConsoleTitle[MAX_PATH], cBuffer[MAX_PATH];
  DWORD dwTitleSaved = GetConsoleTitle(cConsoleTitle, sizeof(cConsoleTitle));
  OSVERSIONINFO ovi;
  ovi.dwOSVersionInfoSize = sizeof(ovi);
  if (GetVersionEx(&ovi) && ovi.dwPlatformId != VER_PLATFORM_WIN32_NT)
  {
    bPlatformNT = false;
    OemToChar(GetMsg(MComparingFiles), cBuffer);
  }
  else
  {
    bPlatformNT = true;
    lstrcpy(cBuffer, GetMsg(MComparingFiles));
  }
  SetConsoleTitle(cBuffer);

  // ������� ���������� ���� ��� �������� �� Esc...
  HANDLE hScreen = Info.SaveScreen(0, 0, -1, -1);
  hConInp = CreateFile("CONIN$", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  // ������ ����� �������� ������� �� �������...
  memset(&Diff, 0, sizeof(Diff));
  memset(&Work, 0, sizeof(Work));
  bBrokenByEsc = false;
  bool bDifferenceNotFound = false;
  bStartMsg = bBuildCacheResult = true;
  static DWORD dwTicks = 0;

  if (Opt.CompareContents)
  {
    ABuf = (char*)malloc(Opt.BufSize);
    PBuf = (char*)malloc(Opt.BufSize);
    if (!ABuf || !PBuf)
    {
      ErrorMsg(MNoMemTitle, MNoMemBody);
      if (ABuf) free(ABuf);
      if (PBuf) free(PBuf);
      return hPlugin;
    }
  }
  // ...������ ������� ������ ��� ���� ������ � ����������-���������...
  if (Opt.Panel)
  {
    Panel = new CmpPanel();
    if ((hPlugin=Panel) == 0)
    {
      ErrorMsg(MNoMemTitle, MNoMemBody);
      return INVALID_HANDLE_VALUE;
    }
  }
  // ����������
  dwTicks = GetTickCount();
  bDifferenceNotFound = CompareDirs(&AInfo, &PInfo, true, 0);
  if (ABuf) free(ABuf);
  if (PBuf) free(PBuf);

  // ���������-������� �� ������� - ������ �� �����...
  if (bBrokenByEsc || (Opt.Panel && !Panel->CheckItem()))
  {
    if (Panel) delete Panel;
    Opt.Panel = 0;
    hPlugin = INVALID_HANDLE_VALUE;
  }

  // ����������� ������ ����...
  CloseHandle(hConInp);
  Info.RestoreScreen(hScreen);
  // ����������� ��������� ������� ����...
  if (dwTitleSaved)
    SetConsoleTitle(cConsoleTitle);

  // �������� ����� � �������������� ������. ���� ����� ���������� ���������...
  if (!bBrokenByEsc && !Opt.Panel)
  {
    if (Opt.Sound && (GetTickCount()-dwTicks > 10000)) MessageBeep(MB_ICONASTERISK);
    if (Opt.CompareName)
    {
      Info.Control(INVALID_HANDLE_VALUE, FCTL_SETSELECTION, &AInfo);
      Info.Control(INVALID_HANDLE_VALUE, FCTL_SETANOTHERSELECTION, &PInfo);
      Info.Control(INVALID_HANDLE_VALUE, FCTL_REDRAWPANEL, 0);
      Info.Control(INVALID_HANDLE_VALUE, FCTL_REDRAWANOTHERPANEL, 0);

      if (bDifferenceNotFound && Opt.ShowMessage)
      {
        const char *MsgItems[] = { GetMsg(MNoDiffTitle), GetMsg(MNoDiffBody), GetMsg(MOK) };
        Info.Message( Info.ModuleNumber,
                      0,
                      0,
                      MsgItems,
                      sizeof(MsgItems) / sizeof(MsgItems[0]),
                      1 );
      }
    }
    else
    // ��������� ���� ���� ���������...
    {
      char fileA[NM], fileP[NM];
      lstrcpy(fileA, BuildFullFilename(AInfo.CurDir, AInfo.PanelItems[AInfo.CurrentItem].FindData.cFileName));
      lstrcpy(fileP, BuildFullFilename(PInfo.CurDir, PInfo.PanelItems[PInfo.CurrentItem].FindData.cFileName));

      const char *MsgItems[] = {
        GetMsg(MUnderCursor),
        fileA,
        fileP,
        (bDifferenceNotFound ? GetMsg(MNoDiff) : (Opt.RunDiffProgram?GetMsg(MRunDiffProgram):GetMsg(MDiff))),
        "\x01"
      };
      int Ret= Info.Message( Info.ModuleNumber,
                             (bDifferenceNotFound ? FMSG_MB_OK :
                                 (Opt.RunDiffProgram?FMSG_WARNING|FMSG_MB_YESNO:FMSG_WARNING|FMSG_MB_OK)),
                             0,
                             MsgItems,
                             sizeof(MsgItems) / sizeof(MsgItems[0]),
                             0 );
      // ���� �����, �� �������� ������� ������� ���������...
      if (Opt.RunDiffProgram && !bDifferenceNotFound && !Ret)
      {
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        char Command[4096];
        FSF.sprintf(Command, "%s %s %s", Opt.PathDiffProgram, FSF.QuoteSpaceOnly(fileA), FSF.QuoteSpaceOnly(fileP));
        CreateProcess(0, Command, 0, 0, false, 0, 0, 0, &si, &pi);
      }
    }
  }
  return hPlugin;
}

/****************************************************************************
 * ��� ������� FAR �������� � ������ ������� ����� ��������� ������ �������
 * (�.�. ������ ���� hPlugin != INVALID_HANDLE_VALUE)
 * ��������� FAR� ���������� ��������� ������
 ****************************************************************************/
void WINAPI _export GetOpenPluginInfo(HANDLE hPlugin, struct OpenPluginInfo *Info)
{
  CmpPanel *Panel = (CmpPanel *)hPlugin;
  Panel->GetOpenPluginInfo(Info);
}

/****************************************************************************
 * ��� ������� FAR �������� �� ������ ������� ����� ������� ������ �������
 * (�.�. ������ ���� hPlugin != INVALID_HANDLE_VALUE)
 * ��������� FAR� ������� �������� ������
 ****************************************************************************/
int WINAPI _export GetFindData(HANDLE hPlugin, struct PluginPanelItem **pPanelItem, int *pItemsNumber, int OpMode)
{
  CmpPanel *Panel = (CmpPanel *)hPlugin;
  return (Panel->GetFindData(pPanelItem, pItemsNumber, OpMode));
}

/****************************************************************************
 * ��� ������� FAR �������� ����� �������� ������ �������
 ****************************************************************************/
void WINAPI _export ClosePlugin(HANDLE hPlugin)
{
  delete (CmpPanel *)hPlugin;
}

/****************************************************************************
 * ��� ������� FAR �������� ����� ��������� �������
 ****************************************************************************/
void WINAPI _export ExitFAR(void)
{
  //��������� ������ � ������ �������� �������
#ifdef CACHE
  if (Cache.iCount)
  {
    for (int i=Cache.iCount-1; i>=0; i--)
    {
      if (Cache.RCD[i]->RCI) free(Cache.RCD[i]->RCI);
      Cache.RCD[i]->RCI=0;
      Cache.RCD[i]->iCount=0;
    }
    if (Cache.RCD) free(Cache.RCD);
    Cache.RCD=0;
    Cache.iCount=0;
  }
#else
  if (CacheResult.rci)
    free(CacheResult.rci);
  CacheResult.rci = 0;
  CacheResult.iCount = 0;
#endif
}
