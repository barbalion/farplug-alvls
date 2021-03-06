/*
TMPPANEL.CPP

Temporary panel main plugin code

*/

#include "TmpPanel.hpp"

TCHAR PluginRootKey[80];
TCHAR TMPPanelDir[NM*5];
unsigned int CurrentCommonPanel;
struct PluginStartupInfo Info;
struct FarStandardFunctions FSF;
BOOL IsOldFAR;
TCHAR StartupCurDir[MAX_PATH];
PluginPanels CommonPanels[COMMONPANELSNUMBER];

#if defined(__GNUC__)
  #define DLLMAINFUNC DllMainCRTStartup
#else
  #define DLLMAINFUNC _DllMainCRTStartup
#endif

#ifdef __cplusplus
extern "C"{
#endif
BOOL WINAPI DLLMAINFUNC(HANDLE hDll,DWORD dwReason,LPVOID lpReserved)
{
  (void) lpReserved;

  if (DLL_PROCESS_ATTACH == dwReason && hDll)
  {
    TCHAR *pf;
    TCHAR temp[NM*5];
    *TMPPanelDir = _T('\0');
    GetModuleFileName((HINSTANCE)hDll, temp, ArraySize(temp));
    if (GetFullPathName(temp, ArraySize(TMPPanelDir), TMPPanelDir, &pf))
      *pf = _T('\0');
  }

  return TRUE;
}
#ifdef __cplusplus
};
#endif

#ifndef UNICODE
#define WITH_ANSI_ARG     ,ANSI
#define WITH_ANSI_PARAM   ,int ANSI
#else
#define WITH_ANSI_ARG
#define WITH_ANSI_PARAM
#endif
static void ProcessList(HANDLE hPlugin, TCHAR *Name, int Mode WITH_ANSI_PARAM);
static void ShowMenuFromList(TCHAR *Name);
static HANDLE OpenPanelFromOutput (TCHAR *argv WITH_ANSI_PARAM);

void WINAPI EXP_NAME(SetStartupInfo)(const struct PluginStartupInfo *Info)
{
  ::Info=*Info;
  IsOldFAR=TRUE;
  if((size_t)Info->StructSize>=sizeof(struct PluginStartupInfo))
  {
    ::FSF=*Info->FSF;
    ::Info.FSF=&::FSF;
    IsOldFAR=FALSE;

    FSF.sprintf(PluginRootKey,_T("%s\\TmpPanel"),Info->RootKey);
    GetOptions();
    StartupOptFullScreenPanel=Opt.FullScreenPanel;
    StartupOptCommonPanel=Opt.CommonPanel;
    CurrentCommonPanel=0;
    memset(CommonPanels, 0, sizeof(CommonPanels));
    CommonPanels[0].Items=(PluginPanelItem*)calloc(1,sizeof(PluginPanelItem));
    Opt.LastSearchResultsPanel = 0;
  }
}


HANDLE WINAPI EXP_NAME(OpenPlugin)(int OpenFrom,INT_PTR Item)
{
  if(IsOldFAR) return(INVALID_HANDLE_VALUE);

  HANDLE hPlugin=INVALID_HANDLE_VALUE;

  GetOptions();
  // ��� �������� � ������� ����� �� <..>
  GetCurrentDirectory(sizeof(StartupCurDir),StartupCurDir);

  StartupOpenFrom=OpenFrom;
  if(OpenFrom==OPEN_COMMANDLINE)
  {
    TCHAR *argv=(TCHAR*)Item;
#ifndef UNICODE
    int ANSI = FALSE;
    #define OPT_COUNT 6
#else
    #define OPT_COUNT 5
#endif
    static const TCHAR ParamsStr[OPT_COUNT][8]=
      {
#ifndef UNICODE
        _T("ansi"),
#endif
                    _T("safe"),_T("any"),_T("replace"),_T("menu"),_T("full")};
    const int *ParamsOpt[OPT_COUNT]=
      {
#ifndef UNICODE
        &ANSI,
#endif
              &Opt.SafeModePanel,&Opt.AnyInPanel,&Opt.Mode,
       &Opt.MenuForFilelist,&Opt.FullScreenPanel};

    while(argv && *argv==_T(' ')) argv++;
    while(lstrlen(argv)>1 && (*argv==_T('+') || *argv==_T('-')))
    {
      int k=0;
      while(argv && *argv!=_T(' '))
      {
        k++;
        argv++;
      }
      TCHAR TMP[MAX_PATH];
      lstrcpyn(TMP,argv-k,k+1);

      for(int i=0;i<OPT_COUNT;i++)
        if(lstrcmpi(TMP+1,ParamsStr[i])==0)
          *(int*)ParamsOpt[i] = *TMP==_T('+');

      if(*(TMP+1)>=_T('0') && *(TMP+1)<=_T('9'))
        CurrentCommonPanel=*(TMP+1)-_T('0');
      while(argv && *argv==_T(' ')) argv++;
    }

    FSF.Trim(argv);
    if(lstrlen(argv))
    {
      if(*argv==_T('<'))
      {
        argv++;
        hPlugin = OpenPanelFromOutput (argv WITH_ANSI_ARG);
      }
      else
      {
        FSF.Unquote(argv);
        TCHAR TmpIn[NM*5],TmpOut[NM*5];
        ExpandEnvStrs(argv,TmpIn,ArraySize(TmpIn));
        TCHAR *p;
        if(SearchPath(TMPPanelDir,TmpIn,NULL,ArraySize(TmpOut),TmpOut,&p) || SearchPath(NULL,TmpIn,NULL,ArraySize(TmpOut),TmpOut,&p))
        {
          if(Opt.MenuForFilelist)
          {
            ShowMenuFromList(TmpOut);
            return(INVALID_HANDLE_VALUE);
          }
          else
          {
            hPlugin=new TmpPanel();
            if(hPlugin==NULL) return(INVALID_HANDLE_VALUE);
            ProcessList(hPlugin, TmpOut, Opt.Mode WITH_ANSI_ARG);
          }
        }
      }
    }
  }
  if(hPlugin==INVALID_HANDLE_VALUE)
  {
    hPlugin=new TmpPanel();
    if(hPlugin==NULL) return(INVALID_HANDLE_VALUE);

    // ������� [x] in mode Branch
    if (Opt.AddToPluginsMenu && Opt.Branch && OpenFrom==OPEN_PLUGINSMENU)
    {
      PanelInfo PInfo;
#ifndef UNICODE
      if (!Info.Control(INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &PInfo))
#else
      if (!Info.Control(PANEL_ACTIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&PInfo))
#endif
        return(INVALID_HANDLE_VALUE);

      ((TmpPanel *)hPlugin)->FindSearchResultsPanel();
      if (Opt.Mode)
      {
        FreePanelItems(CommonPanels[CurrentCommonPanel].Items,
                       CommonPanels[CurrentCommonPanel].ItemsNumber);
        CommonPanels[CurrentCommonPanel].Items=(PluginPanelItem*)malloc(sizeof(PluginPanelItem));
        CommonPanels[CurrentCommonPanel].ItemsNumber=0;
      }
      HANDLE hScreen = ((TmpPanel *)hPlugin)->BeginPutFiles();
      for (int i=0; i<PInfo.SelectedItemsNumber; i++)
      {
#ifndef UNICODE
        if (!((TmpPanel *)hPlugin)->PutOneFile(PInfo.SelectedItems[i]))
        {
          ((TmpPanel *)hPlugin)->CommitPutFiles(hScreen, false);
           return(INVALID_HANDLE_VALUE);
        }
#else
        PluginPanelItem* PPI=(PluginPanelItem*)malloc(Info.Control(PANEL_ACTIVE,FCTL_GETSELECTEDPANELITEM,i,0));
        if (PPI)
        {
          Info.Control(PANEL_ACTIVE,FCTL_GETSELECTEDPANELITEM,i,(LONG_PTR)PPI);
          if (!((TmpPanel *)hPlugin)->PutOneFile(*PPI))
          {
            free(PPI);
            ((TmpPanel *)hPlugin)->CommitPutFiles(hScreen, false);
            return(INVALID_HANDLE_VALUE);
          }
          free(PPI);
        }
#endif
      }
      ((TmpPanel *)hPlugin)->CommitPutFiles(hScreen, true);
    }
  }
  return(hPlugin);
}

static HANDLE OpenPanelFromOutput (TCHAR *argv WITH_ANSI_PARAM)
{
  TCHAR *tempDir=ParseParam(argv);

  BOOL allOK=FALSE;
  TCHAR tempfilename[NM*5],cmdparams[NM*5],fullcmd[NM*5];
  FSF.MkTemp(tempfilename,
#ifdef UNICODE
             ArraySize(tempfilename),
#endif
             _T("FARTMP"));
  lstrcpy(cmdparams,_T("%COMSPEC% /c "));
  lstrcat(cmdparams,argv);
  ExpandEnvStrs(cmdparams,fullcmd,ArraySize(fullcmd));

  SECURITY_ATTRIBUTES sa;
  memset(&sa, 0, sizeof(sa));
  sa.nLength=sizeof(sa);
  sa.bInheritHandle=TRUE;

  HANDLE FileHandle;
  FileHandle=CreateFile(tempfilename,GENERIC_WRITE,FILE_SHARE_READ,
    &sa,CREATE_ALWAYS,FILE_FLAG_SEQUENTIAL_SCAN,NULL);

  if(FileHandle!=INVALID_HANDLE_VALUE)
  {
    STARTUPINFO si;
    memset(&si,0,sizeof(si));
    si.cb=sizeof(si);
    si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput=FileHandle;
    si.hStdError=FileHandle;

    PROCESS_INFORMATION pi;
    memset(&pi,0,sizeof(pi));

    TCHAR SaveDir[NM],workDir[NM];

    if(tempDir)
    {
      GetCurrentDirectory(ArraySize(SaveDir),SaveDir);
      ExpandEnvStrs(tempDir,workDir,ArraySize(workDir));
      SetCurrentDirectory(workDir);
    }

    TCHAR consoleTitle[255];
    DWORD tlen = GetConsoleTitle(consoleTitle, ArraySize(consoleTitle));
    SetConsoleTitle(argv);

    if(CreateProcess(NULL,fullcmd,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi))
    {
      WaitForSingleObject(pi.hProcess,INFINITE);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      allOK=TRUE;
    }
    CloseHandle(FileHandle);

    if(tlen) SetConsoleTitle(consoleTitle);
    if(tempDir) SetCurrentDirectory(SaveDir);
  }
  HANDLE hPlugin = INVALID_HANDLE_VALUE;
  if(allOK)
  {
    hPlugin=new TmpPanel();
    if(hPlugin==NULL)
      return(INVALID_HANDLE_VALUE);
    ProcessList(hPlugin, tempfilename, Opt.Mode WITH_ANSI_ARG);
  }
  DeleteFile(tempfilename);
  return hPlugin;
}

void ReadFileLines(HANDLE hFileMapping, DWORD FileSizeLow, TCHAR **argv, TCHAR *args,
                   UINT *numargs, UINT *numchars WITH_ANSI_PARAM)
{
  *numchars = 0;
  *numargs = 0;

  char *FileData=(char *)MapViewOfFile(hFileMapping,FILE_MAP_READ,0,0,FileSizeLow);
  if (!FileData)
    return;

  TCHAR TMP[MAX_PATH];
  DWORD Len,Pos=0,Size=FileSizeLow;

#ifdef UNICODE
  bool isUTF8=false;
  if (Size >= 3 && FileData[0]=='\xEF' && FileData[1]=='\xBB' && FileData[2]=='\xBF')
  {
    isUTF8=true;
    Pos += 3;
  }
#endif

  while(Pos<Size)
  {
    char c;
    while(Pos<Size && ((c = FileData[Pos]) == '\r' || c == '\n')) Pos++;
    DWORD Off = Pos;
    while(Pos<Size && (c = FileData[Pos]) != '\r' && c != '\n') Pos++;
    Len = Pos - Off;
#ifndef UNICODE
    if(Len >= sizeof(TMP)) Len = sizeof(TMP)-1;
    memcpy(TMP, &FileData[Off], Len);
#else // UNICODE
    Len = MultiByteToWideChar((isUTF8?CP_UTF8:CP_OEMCP), MB_ERR_INVALID_CHARS, &FileData[Off], Len, TMP, ArraySize(TMP)-1);
#endif

    if (!Len) continue;

    TMP[Len]=0;

    FSF.Trim(TMP);
    FSF.Unquote(TMP);

#ifndef UNICODE
    if(ANSI) CharToOem(TMP,TMP);
#endif

    Len = lstrlen(TMP);
    if (!Len) continue;

    if(argv) *argv++ = args;
    if(args)
    {
      lstrcpy(args,TMP);
      args+=Len+1;
    }

    (*numchars)+=Len+1;
    ++*numargs;
  }
  UnmapViewOfFile((LPVOID)FileData);
}

static void ReadFileList (TCHAR *filename, int *argc, TCHAR ***argv WITH_ANSI_PARAM)
{
  *argc = 0;
  *argv = NULL;

  HANDLE hFile=CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,NULL,
    OPEN_EXISTING,0,NULL);

  if(hFile != INVALID_HANDLE_VALUE)
  {
    DWORD FileSizeLow=GetFileSize(hFile,NULL);
    HANDLE hFileMapping=CreateFileMapping(hFile,NULL,PAGE_READONLY,0,0,NULL);
    CloseHandle(hFile);

    if(hFileMapping != NULL)
    {
      UINT i;
      ReadFileLines(hFileMapping, FileSizeLow, NULL, NULL, (UINT*)argc, &i WITH_ANSI_ARG);
      *argv = (TCHAR**)malloc(*argc*sizeof(TCHAR*) + i*sizeof(TCHAR));
      ReadFileLines(hFileMapping, FileSizeLow, *argv, (TCHAR*)&(*argv)[*argc],
                    (UINT*)argc, &i WITH_ANSI_ARG);
      CloseHandle(hFileMapping);
    }
  }
}

static void ProcessList(HANDLE hPlugin, TCHAR *Name, int Mode WITH_ANSI_PARAM)
{
  if(Mode)
  {
    FreePanelItems(CommonPanels[CurrentCommonPanel].Items,
                   CommonPanels[CurrentCommonPanel].ItemsNumber);
    CommonPanels[CurrentCommonPanel].Items=(PluginPanelItem*)calloc(1,sizeof(PluginPanelItem));
    CommonPanels[CurrentCommonPanel].ItemsNumber=0;
  }
  TmpPanel *Panel=(TmpPanel*)hPlugin;

  int argc;
  TCHAR **argv;
  ReadFileList (Name, &argc, &argv WITH_ANSI_ARG);

  HANDLE hScreen = Panel->BeginPutFiles();

  struct PluginPanelItem ppi;
  memset(&ppi,0,sizeof(ppi));
  for(UINT i=0;(int)i<argc;i++)
    if(Panel->CheckForCorrect(argv[i],&ppi.FindData,Opt.AnyInPanel))
      Panel->PutOneFile(ppi);

  Panel->CommitPutFiles (hScreen, TRUE);
  if (argv)
    free(argv);
}


void ShowMenuFromList(TCHAR *Name)
{
  int argc;
  TCHAR **argv;
#ifndef UNICODE
  static const int ANSI = FALSE;
#endif
  ReadFileList (Name, &argc, &argv WITH_ANSI_ARG);

  FarMenuItem *fmi=(FarMenuItem*)malloc(argc*sizeof(FarMenuItem));
  if (fmi)
  {
    TCHAR TMP[MAX_PATH];
    int i;
    for(i=0;i<argc;++i)
    {
      TCHAR *param,*p=TMP;
      ExpandEnvStrs(argv[i],TMP,ArraySize(TMP));
      param=ParseParam(p);
      FSF.TruncStr(param?param:p,67);

#ifndef UNICODE
      strncpy(fmi[i].Text,param?param:p,sizeof(fmi[i].Text)-1);
#else
      fmi[i].Text = wcsdup(param?param:p);
#endif

      fmi[i].Separator=!lstrcmp(param,_T("-"));
    }
//    fmi[0].Selected=TRUE;

    TCHAR Title[MAX_PATH];
    FSF.ProcessName(FSF.PointToName(Name),lstrcpy(Title,_T("*.")),
#ifdef UNICODE
                    ArraySize(Title),
#endif
                    PN_GENERATENAME);
    FSF.TruncPathStr(Title,64);

    int BreakCode;
    static const int BreakKeys[2]={MAKELONG(VK_RETURN,PKF_SHIFT),0};

    int ExitCode=Info.Menu(Info.ModuleNumber, -1, -1, 0,
      FMENU_WRAPMODE, Title, NULL, _T("Contents"),
      &BreakKeys[0], &BreakCode, fmi, argc);

#ifdef UNICODE
    for(i=0;i<argc;++i)
      if(fmi[i].Text)
        free((void*)fmi[i].Text);
#endif

    free(fmi);

    if((unsigned)ExitCode<(unsigned)argc)
    {
      ExpandEnvStrs(argv[ExitCode],TMP,ArraySize(TMP));
      TCHAR *p=TMP;
      ParseParam(p);

      FAR_FIND_DATA FindData;
      int bShellExecute=BreakCode!=-1;

      if(!bShellExecute)
      {
        if(TmpPanel::CheckForCorrect(p,&FindData,FALSE))
        {
          if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
#ifndef UNICODE
            Info.Control(INVALID_HANDLE_VALUE,FCTL_SETPANELDIR,p);
#else
            Info.Control(INVALID_HANDLE_VALUE,FCTL_SETPANELDIR,0,(LONG_PTR)p);
#endif
          else
            bShellExecute=TRUE;
        }
        else
#ifndef UNICODE
          Info.Control(INVALID_HANDLE_VALUE,FCTL_SETCMDLINE,p);
#else
          Info.Control(PANEL_ACTIVE,FCTL_SETCMDLINE,0,(LONG_PTR)p);
#endif
      }
      if(bShellExecute)
        ShellExecute(NULL,_T("open"),p,NULL,NULL,SW_SHOW);
    }
  }
  if (argv)
    free(argv);
}


#ifndef UNICODE
#define _CONST
#define _OPARG
#else
#define _CONST const
#define _OPARG ,int
#endif
HANDLE WINAPI EXP_NAME(OpenFilePlugin)(_CONST TCHAR *Name,const unsigned char *,int DataSize _OPARG)
#undef _OPARG
#undef _CONST
{
#ifndef UNICODE
#define PNAME_ARG Name
#define pName     Name
#else
  wchar_t pName[MAX_PATH];
  lstrcpy(pName, Name);
#define PNAME_ARG pName,ArraySize(pName)
#endif

  if(IsOldFAR || !DataSize || !FSF.ProcessName(Opt.Mask,PNAME_ARG,PN_CMPNAMELIST))
    return(INVALID_HANDLE_VALUE);
#undef PNAME_ARG

  //save PanelMode
  GetOptions();
  // ��� �������� � ������� ����� �� <..>
  GetCurrentDirectory(sizeof(StartupCurDir),StartupCurDir);

  if(!Opt.MenuForFilelist)
  {
    HANDLE hPlugin=new TmpPanel();
    if(hPlugin == NULL) return(INVALID_HANDLE_VALUE);
#ifndef UNICODE
    static const int ANSI = FALSE;
#endif
    ProcessList(hPlugin, pName, Opt.Mode WITH_ANSI_ARG);
    return(hPlugin);
  }
  else
  {
    ShowMenuFromList(pName);
    return(INVALID_HANDLE_VALUE);
  }
#undef pName
}

void WINAPI EXP_NAME(ClosePlugin)(HANDLE hPlugin)
{
  delete(TmpPanel *)hPlugin;
}


void WINAPI EXP_NAME(ExitFAR)()
{
  for(int i=0;i<COMMONPANELSNUMBER;++i)
    FreePanelItems(CommonPanels[i].Items, CommonPanels[i].ItemsNumber);
}


int WINAPI EXP_NAME(GetFindData)(HANDLE hPlugin,struct PluginPanelItem **pPanelItem,int *pItemsNumber,int OpMode)
{
  TmpPanel *Panel=(TmpPanel *)hPlugin;
  return(Panel->GetFindData(pPanelItem,pItemsNumber,OpMode));
}


void WINAPI EXP_NAME(GetPluginInfo)(struct PluginInfo *Info)
{
  Info->StructSize=sizeof(*Info);
  Info->Flags=PF_PRELOAD;
  static const TCHAR *DiskMenuStrings[1];
  DiskMenuStrings[0]=GetMsg(MDiskMenuString);
  Info->DiskMenuStrings=DiskMenuStrings;
  static int DiskMenuNumbers[1];
  DiskMenuNumbers[0]=FSF.atoi(Opt.DisksMenuDigit);
  Info->DiskMenuNumbers=DiskMenuNumbers;
  Info->DiskMenuStringsNumber=Opt.AddToDisksMenu?ArraySize(DiskMenuStrings):0;
  static const TCHAR *PluginMenuStrings[1];
  PluginMenuStrings[0]=Opt.Branch?GetMsg(MPluginMenuStringBranch):GetMsg(MTempPanel);
  Info->PluginMenuStrings=Opt.AddToPluginsMenu?PluginMenuStrings:NULL;
  Info->PluginMenuStringsNumber=Opt.AddToPluginsMenu?ArraySize(PluginMenuStrings):0;
  static const TCHAR *PluginCfgStrings[1];
  PluginCfgStrings[0]=GetMsg(MTempPanel);
  Info->PluginConfigStrings=PluginCfgStrings;
  Info->PluginConfigStringsNumber=ArraySize(PluginCfgStrings);
  Info->CommandPrefix=Opt.Prefix;
}


void WINAPI EXP_NAME(GetOpenPluginInfo)(HANDLE hPlugin,struct OpenPluginInfo *Info)
{
  TmpPanel *Panel=(TmpPanel *)hPlugin;
  Panel->GetOpenPluginInfo(Info);
}


int WINAPI EXP_NAME(SetDirectory)(HANDLE hPlugin,const TCHAR *Dir,int OpMode)
{
  TmpPanel *Panel=(TmpPanel *)hPlugin;
  return(Panel->SetDirectory(Dir,OpMode));
}


int WINAPI EXP_NAME(PutFiles)(HANDLE hPlugin,struct PluginPanelItem *PanelItem,
                              int ItemsNumber,int Move,int OpMode)
{
  TmpPanel *Panel=(TmpPanel *)hPlugin;
  return(Panel->PutFiles(PanelItem,ItemsNumber,Move,OpMode));
}


int WINAPI EXP_NAME(SetFindList)(HANDLE hPlugin,const struct PluginPanelItem *PanelItem,int ItemsNumber)
{
  TmpPanel *Panel=(TmpPanel *)hPlugin;
  return(Panel->SetFindList(PanelItem,ItemsNumber));
}


int WINAPI EXP_NAME(ProcessEvent)(HANDLE hPlugin,int Event,void *Param)
{
  TmpPanel *Panel=(TmpPanel *)hPlugin;
  return(Panel->ProcessEvent(Event,Param));
}


int WINAPI EXP_NAME(ProcessKey)(HANDLE hPlugin,int Key,unsigned int ControlState)
{
  TmpPanel *Panel=(TmpPanel *)hPlugin;
  return(Panel->ProcessKey(Key,ControlState));
}


int WINAPI EXP_NAME(Configure)(int ItemNumber)
{
  return(!IsOldFAR && !ItemNumber?Config():FALSE);
}

int WINAPI EXP_NAME(GetMinFarVersion)(void)
{
#ifndef UNICODE
  return(MAKEFARVERSION(1,70,2087));
#else
  return(MAKEFARVERSION(2,0,863));
#endif
}
