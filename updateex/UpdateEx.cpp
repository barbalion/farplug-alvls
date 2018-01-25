#include "headers.hpp"

#include "archiver.h"
#include <initguid.h>
#include "guid.hpp"
#include "Imports.hpp"

#include "UpdateEx.hpp"
#include "lng.hpp"
#include "tinyxml.hpp"

#include "Console.hpp"
#include "CursorPos.hpp"
#include "HideCursor.hpp"
#include "TextColor.hpp"

//http://www.farmanager.com/files/update3.php?p=32
//http://www.farmanager.com/nightly/update3.php?p=32
LPCWSTR FarRemoteSrv=L"www.farmanager.com";
LPCWSTR FarRemotePath1=L"/files/";
LPCWSTR FarRemotePath2=L"/nightly/";
LPCWSTR FarUpdateFile=L"update3.php";
LPCWSTR phpRequest=
#ifdef _WIN64
                   L"?p=64";
#else
                   L"?p=32";
#endif

//http://plugring.farmanager.com/command.php
//http://plugring.farmanager.com/download.php?fid=1082
LPCWSTR PlugRemoteSrv=L"plugring.farmanager.com";
LPCWSTR PlugRemotePath1=L"/";
LPCWSTR PlugRemotePath2=L"/download.php?fid=";
LPCWSTR PlugUpdateFile=L"command.php";

enum MODULEINFOFLAG {
	NONE     = 0x000,
	ERR      = 0x001,  // ������
	STD      = 0x002,  // ����������� ����
	ANSI     = 0x004,  // ansi-����
	SKIP     = 0x008,  // �������� ����������
	UPD      = 0x010,  // ����� ���������
	INFO     = 0x020,  // ��������� � ���� ���� �� ����������
	ARC      = 0x040,  // ��������� ����� ��� ����������
	NEW      = 0x080,  // �����, ��� �� ����������
	ACTIVE   = 0x100,  // �������� (�������� � ������)
	UNDO     = 0x200,  // ���� ������ ��� ������
};

struct ModuleInfo
{
	DWORD ID;  // ����� � ����� �������
	GUID Guid;
	DWORD Flags;
	struct VersionInfo Version;
	wchar_t pid[64];  // plug ID
	wchar_t Title[64];
	wchar_t Description[2048];
	wchar_t Author[MAX_PATH];
	wchar_t ModuleName[MAX_PATH];
	wchar_t Date[11];
	wchar_t Changelog[2048];
	wchar_t fid[64];  // file ID
	wchar_t Downloads[64]; // ���������� ����������
	struct Data
	{
		struct VersionInfo MinFarVersion;
		struct VersionInfo Version;
		wchar_t ArcName[MAX_PATH];
		wchar_t Date[11];
	} New, Undo;
};

enum {
	MODE_UPD  = 0,
	MODE_NEW  = 1,
	MODE_UNDO = 2
};

struct OPT
{
	DWORD Mode;
	DWORD Stable;
	DWORD Auto;
	DWORD TrayNotify;
	DWORD Wait;
	DWORD ShowDisable;
	DWORD Date;
	DWORD SaveFarAfterInstall;    //0- ���, 1-��, 2-��, �� ������ 2 ���������
	DWORD SavePlugAfterInstall;
	DWORD ShowDate;
	DWORD ShowDraw;
	DWORD Autoload;
	DWORD Proxy;
	wchar_t ProxyName[MAX_PATH];
	wchar_t ProxyUser[MAX_PATH];
	wchar_t ProxyPass[MAX_PATH];
	wchar_t TempDirectory[MAX_PATH]; // ��� ������ "��� ����"
	wchar_t DateFrom[11];
	wchar_t DateTo[11];
};

struct IPC
{
	wchar_t FarParams[MAX_PATH*4];
	wchar_t TempDirectory[MAX_PATH];
	wchar_t Config[MAX_PATH];
	wchar_t Cache[MAX_PATH];
	wchar_t FarUpdateList[MAX_PATH];
	wchar_t PlugUpdateList[MAX_PATH];
	OPT opt;
	//---
	ModuleInfo *Modules;
	size_t CountModules;
} ipc;

void FreeModulesInfo()
{
	if (ipc.Modules) free(ipc.Modules);
	ipc.Modules=nullptr;
	ipc.CountModules=0;
}

enum STATUS
{
	S_NONE=0,
	S_CANTGETINSTALLINFO,
	S_CANTCREATTMP,
	S_CANTGETFARLIST,
	S_CANTGETPLUGLIST,
	S_CANTGETFARUPDINFO,
	S_CANTGETPLUGUPDINFO,
	S_UPTODATE,
	S_UPDATE,
	S_DOWNLOAD,
	S_CANTCONNECT,
	S_COMPLET,
};

enum EVENT
{
	E_LOADPLUGINS,
	E_ASKUPD,
	E_EXIT,
	E_CANTCOMPLETE
};

struct EventStruct
{
	EVENT Event;
	LPVOID Data;
	bool *Result;
};

bool NeedRestart=false;
bool ExitFAR=false;
DWORD Status=S_NONE;
size_t CountListItems=0;
size_t CountUpdate=0;
size_t CountDownload=0;

wchar_t PluginModule[MAX_PATH];

SYSTEMTIME SavedTime;

HANDLE hThread=nullptr;
HANDLE hRunDll=nullptr;
HANDLE hDlg=nullptr;
HANDLE StopEvent=nullptr;
HANDLE UnlockEvent=nullptr;
HANDLE WaitEvent=nullptr;
HANDLE hNotifyThread=nullptr;
HANDLE hAutoloadThread_1=nullptr, hAutoloadThread_2=nullptr;
HANDLE hExitAutoloadThreadEvent=nullptr;

CRITICAL_SECTION cs;

PluginStartupInfo Info;
FarStandardFunctions FSF;

void SetStatus(DWORD Set)
{
	EnterCriticalSection(&cs);
	Status=Set;
	LeaveCriticalSection(&cs);
}

DWORD GetStatus()
{
	EnterCriticalSection(&cs);
	DWORD ret=Status;
	LeaveCriticalSection(&cs);
	return ret;
}

INT mprintf(LPCWSTR format,...)
{
	va_list argptr;
	va_start(argptr,format);
	wchar_t buff[1024];
	DWORD n=wvsprintf(buff,format,argptr);
	va_end(argptr);
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),buff,n,&n,nullptr);
	return n;
}

template<class T>T StringToNumber(LPCWSTR String, T &Number)
{
	Number=0;
	for(LPCWSTR p=String;p&&*p;p++)
	{
		if(*p>=L'0'&&*p<=L'9')
			Number=Number*10+(*p-L'0');
		else break;
	}
	return Number;
}

bool Clean()
{
	if (!ipc.opt.SavePlugAfterInstall && !ipc.opt.SaveFarAfterInstall)
		DeleteFile(ipc.Cache);
	DeleteFile(PluginModule);
	RemoveDirectory(ipc.TempDirectory);
	return true;
}

bool IsTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	EnterCriticalSection(&cs);
	bool Result=st.wYear!=SavedTime.wYear||st.wMonth!=SavedTime.wMonth||st.wDay!=SavedTime.wDay||(st.wHour-SavedTime.wHour)>=1;
	LeaveCriticalSection(&cs);
	return Result;
}

VOID SaveTime()
{
	EnterCriticalSection(&cs);
	GetLocalTime(&SavedTime);
	LeaveCriticalSection(&cs);
}

VOID CleanTime()
{
	EnterCriticalSection(&cs);
	memset(&SavedTime,0,sizeof(SavedTime));
	LeaveCriticalSection(&cs);
}

wchar_t *GetStrFileTime(wchar_t *Time, FILETIME *LastWriteTime)
{
	SYSTEMTIME ModificTime;
	FILETIME LocalTime;
	FileTimeToLocalFileTime(LastWriteTime,&LocalTime);
	FileTimeToSystemTime(&LocalTime,&ModificTime);
	// ��� Time ���������� [11] !!!
	if (Time)
		FSF.sprintf(Time,L"%04d-%02d-%02d",ModificTime.wYear,ModificTime.wMonth,ModificTime.wDay);
	return Time;
}

// 0000.00.00 => 00.00.0000
void CopyReverseTime(wchar_t *out,const wchar_t *in)
{
	if (lstrlen(in)>=10)
	{
		memcpy(&out[0],&in[8],2*sizeof(wchar_t));
		memcpy(&out[2],&in[4],4*sizeof(wchar_t));
		memcpy(&out[6],&in[0],4*sizeof(wchar_t));
		out[10]=0;
	}
	else
		out[0]=0;
}

// 00.00.0000 => 0000.00.00
void CopyReverseTime2(wchar_t *out,const wchar_t *in)
{
	if (lstrlen(in)>=10)
	{
		memcpy(&out[0],&in[6],4*sizeof(wchar_t));
		memcpy(&out[4],&in[2],4*sizeof(wchar_t));
		memcpy(&out[8],&in[0],2*sizeof(wchar_t));
		out[10]=0;
	}
	else
		out[0]=0;
}

wchar_t *GetModuleDir(const wchar_t *Path, wchar_t *Dir)
{
	lstrcpy(Dir,Path);
	*(StrRChr(Dir,nullptr,L'\\')+1)=0;
	return Dir;
}

wchar_t *GetNewModuleDir(const wchar_t *ArcName, wchar_t *Dir)
{
	GetModuleDir(ipc.Modules[0].ModuleName,Dir);
	lstrcat(Dir,L"Plugins\\");
	if (ArcName)
	{
		wchar_t tmp[MAX_PATH];
		lstrcpy(tmp,ArcName);
		wchar_t *p=StrPBrk(tmp,L"._-");
		if (p) *p=0;
		lstrcat(Dir,tmp);
		lstrcat(Dir,L"\\");
	}
	else
		lstrcat(Dir,L"??\\");
	return Dir;
}

struct STDPLUG
{
	size_t i;
	const GUID *Guid;
	wchar_t *Changelog;
} StdPlug[]= {
	{0, &FarGuid,        L"http://farmanager.googlecode.com/svn/trunk/unicode_far/changelog"},
	{1, &AlignGuid,      L"align"},
	{2, &ArcliteGuid,    L"arclite"},
	{3, &AutowrapGuid,   L"autowrap"},
	{4, &BracketsGuid,   L"brackets"},
	{5, &CompareGuid,    L"compare"},
	{6, &DrawlineGuid,   L"drawline"},
	{7, &EditcaseGuid,   L"editcase"},
	{8, &EmenuGuid,      L"emenu"},
	{9, &FarcmdsGuid,    L"farcmds"},
	{10,&FilecaseGuid,   L"filecase"},
	{11,&HlfviewerGuid,  L"hlfviewer"},
	{12,&LuamacroGuid,   L"luamacro"},
	{13,&NetworkGuid,    L"network"},
	{14,&ProclistGuid,   L"proclist"},
	{15,&TmppanelGuid,   L"tmppanel"},
	{16,&FarcolorerGuid, L"http://raw.github.com/colorer/FarColorer/master/docs/history.ru.txt"},
	{17,&NetboxGuid,     L"http://raw.github.com/michaellukashov/Far-NetBox/master/ChangeLog"}
};

bool IsStdPlug(GUID PlugGuid)
{
	for (size_t i=0; i<=17; i++)
		if (*StdPlug[i].Guid==PlugGuid)
			return true;
	return false;
}

bool StrToGuid(const wchar_t *Value,GUID &Guid)
{
	return (UuidFromString((unsigned short*)Value,&Guid)==RPC_S_OK)?true:false;
}

wchar_t *GuidToStr(const GUID& Guid, wchar_t *Value)
{
	if (Value)
		wsprintf(Value,L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",Guid.Data1,Guid.Data2,Guid.Data3,Guid.Data4[0],Guid.Data4[1],Guid.Data4[2],Guid.Data4[3],Guid.Data4[4],Guid.Data4[5],Guid.Data4[6],Guid.Data4[7]);
	return Value;
}

bool CmpListGuid(wchar_t *ListGuid,GUID &Guid)
{
	bool ret=false;
	if (ListGuid&&ListGuid[0])
	{
		wchar_t guid[37];
		GuidToStr(Guid,guid);
		for (wchar_t *p=ListGuid; *p; p+=(36+1))
		{
			if (!FSF.LStrnicmp(p,guid,36))
			{
				ret=true;
				break;
			}
		}
	}
	return ret;
}

wchar_t *ReplaseSymbol(wchar_t *Str)
{
	if (Str)
	{
		int lenStr=lstrlen(Str);
		wchar_t *Buf=(wchar_t*)malloc((lenStr+1)*sizeof(wchar_t));
		if (Buf)
		{
			int i=0,j=0;
			// ������ ������ ��������
			for ( ; i<lenStr; )
			{
				if (!FSF.LStrnicmp(Str+i,L"&amp;",5))
				{
					Buf[i++]=Str[j];
					j+=5;
				}
				else if (!FSF.LStrnicmp(Str+i,L"&apos;",6))
				{
					Buf[i++]=L'\'';
					j+=6;
				}
				else if (!FSF.LStrnicmp(Str+i,L"&quot;",6))
				{
					Buf[i++]=L'\"';
					j+=6;
				}
				else
					Buf[i++]=Str[j++];
			}
			Buf[i]=0;
			lstrcpy(Str,Buf);
			free(Buf);
		}
	}
	return Str;
}

wchar_t *CharToWChar(const char *str)
{
	wchar_t *buf=nullptr;
	if (str)
	{
		int size=MultiByteToWideChar(CP_UTF8,0,str,-1,0,0);
		buf=(wchar_t*)malloc(size*sizeof(wchar_t));
		if (buf) MultiByteToWideChar(CP_UTF8,0,str,-1,buf,size);
	}
	return buf;
}

bool NeedUpdate(VersionInfo &Cur,VersionInfo &New, bool EqualBuild=false)
{
	return (New.Major>Cur.Major) ||
	((New.Major==Cur.Major)&&(New.Minor>Cur.Minor)) ||
	((New.Major==Cur.Major)&&(New.Minor==Cur.Minor)&&(New.Revision>Cur.Revision)) ||
	((New.Major==Cur.Major)&&(New.Minor==Cur.Minor)&&(New.Revision==Cur.Revision)&&(EqualBuild?New.Build>=Cur.Build:New.Build>Cur.Build));
}

bool CheckFarVer(VersionInfo &Cur,bool isANSI=false)
{
	if (!isANSI)
	{
		// �������� ���� �� ���� 3410
		VersionInfo FarVer3410=MAKEFARVERSION(3,0,0,3410,VS_RELEASE);
		return NeedUpdate(FarVer3410,Cur,true);
	}
	else
		// �������� ���� �� ANSI
		return (Cur.Major==1 && Cur.Minor!=80);
}

bool ParentIsFar()
{
	typedef struct _smPROCESS_BASIC_INFORMATION {
		LONG ExitStatus;
		PPEB PebBaseAddress;
		ULONG_PTR AffinityMask;
		LONG BasePriority;
		ULONG_PTR UniqueProcessId;
		ULONG_PTR InheritedFromUniqueProcessId;
	} smPROCESS_BASIC_INFORMATION, *smPPROCESS_BASIC_INFORMATION;

	HANDLE hFarDup;
	smPROCESS_BASIC_INFORMATION processInfo;
	DWORD ret;
	bool isFar=false;

	DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&hFarDup,0,FALSE,DUPLICATE_SAME_ACCESS);

	if (ifn.NtQueryInformationProcess(hFarDup,ProcessBasicInformation,&processInfo,sizeof(processInfo),&ret)==NO_ERROR)
	{
		HANDLE hParent=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,(DWORD)processInfo.InheritedFromUniqueProcessId);
		if (hParent)
		{
			wchar_t FileName[MAX_PATH];
			DWORD sz=MAX_PATH;
			if (GetModuleFileNameEx(hParent,nullptr,FileName,sz))
			{
				if (!FSF.LStricmp(FileName,ipc.Modules[0].ModuleName))
					isFar=true;
			}
			else if (ifn.QueryFullProcessImageNameW(hParent,0,FileName,&sz))
			{
				if (!FSF.LStricmp(FileName,ipc.Modules[0].ModuleName))
					isFar=true;
			}
			CloseHandle(hParent);
		}
	}
	CloseHandle(hFarDup);
	return isFar;
}

VOID StartUpdate(bool Thread)
{
	if (ParentIsFar())
	{
		MessageBeep(MB_ICONASTERISK);
		return;
	}

	DWORD RunDllExitCode=0;
	GetExitCodeProcess(hRunDll,&RunDllExitCode);
	if(RunDllExitCode==STILL_ACTIVE)
	{
		if (!Thread)
		{
			LPCWSTR Items[]={MSG(MName),MSG(MCantCompleteUpd),MSG(MExitFAR)};
			Info.Message(&MainGuid,&MsgCantCompleteUpdGuid, FMSG_WARNING|FMSG_MB_OK, nullptr, Items, ARRAYSIZE(Items), 0);
		}
		else
		{
			HANDLE hEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
			EventStruct es={E_CANTCOMPLETE,hEvent};
			Info.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, &es);
			WaitForSingleObject(hEvent,INFINITE);
			CloseHandle(hEvent);
		}
	}
	else if(NeedRestart)
	{
		HANDLE ProcDup;
		DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&ProcDup,0,TRUE,DUPLICATE_SAME_ACCESS);

		wchar_t cmdline[MAX_PATH];

		int NumArgs=0;
		LPWSTR *Argv=CommandLineToArgvW(GetCommandLine(), &NumArgs);
		*ipc.FarParams=0;
		for(int i=1;i<NumArgs;i++)
		{
			lstrcat(ipc.FarParams,Argv[i]);
			if(i<NumArgs-1)
				lstrcat(ipc.FarParams,L" ");
		}
		LocalFree(Argv);

		bool bSelf=false;
		for (size_t i=0; i<ipc.CountModules; i++)
		{
			if (ipc.Modules[i].Guid==MainGuid)
			{
				if (ipc.Modules[i].Flags&UPD)
				{
					if (CopyFile(ipc.Modules[i].ModuleName,PluginModule,FALSE))
						bSelf=true;
				}
				break;
			}
		}

		wchar_t WinDir[MAX_PATH];
		GetWindowsDirectory(WinDir,ARRAYSIZE(WinDir));
		BOOL IsWow64=FALSE;
		FSF.sprintf(cmdline,L"%s\\%s\\rundll32.exe \"%s\",RestartFAR %I64d %I64d",WinDir,ifn.IsWow64Process(GetCurrentProcess(),&IsWow64)&&IsWow64?L"SysWOW64":L"System32",bSelf?PluginModule:Info.ModuleName,reinterpret_cast<INT64>(ProcDup),reinterpret_cast<INT64>(&ipc));

		STARTUPINFO si={sizeof(si)};
		PROCESS_INFORMATION pi;

		BOOL Created=CreateProcess(nullptr,cmdline,nullptr,nullptr,TRUE,0,nullptr,nullptr,&si,&pi);

		if(Created)
		{
			hRunDll=pi.hProcess;
			CloseHandle(pi.hThread);
			if (!Thread)
			{
				LPCWSTR Items[]={MSG(MName),MSG(MExitFAR),MSG(MExitFARAsk)};
				if ((ExitFAR=!Info.Message(&MainGuid,&MsgExitFARGuid, FMSG_MB_YESNO, nullptr, Items, ARRAYSIZE(Items), 0))==0)
				{
					Console console;
					TextColor color(FOREGROUND_GREEN|FOREGROUND_INTENSITY);
					mprintf(MSG(MExitFAR));
					mprintf(L"\n\n");
				}
			}
			else
			{
				HANDLE hEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
				EventStruct es={E_EXIT,hEvent};
				Info.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, &es);
				WaitForSingleObject(hEvent,INFINITE);
				CloseHandle(hEvent);
			}
		}
		else
		{
			if (!Thread)
			{
				LPCWSTR Items[]={MSG(MName),MSG(MCantCreateProcess)};
				Info.Message(&MainGuid,&MsgCantCreateProcessGuid, FMSG_MB_OK|FMSG_ERRORTYPE, nullptr, Items, ARRAYSIZE(Items), 0);
			}
		}
	}
}

bool GetCurrentModuleVersion(LPCTSTR Module,VersionInfo &vi)
{
	vi.Major=vi.Minor=vi.Revision=vi.Build=0;
	bool ret=false;
	DWORD dwHandle;
	DWORD dwSize=GetFileVersionInfoSize(Module,&dwHandle);
	if(dwSize)
	{
		LPVOID Data=malloc(dwSize);
		if(Data)
		{
			if(GetFileVersionInfo(Module,NULL,dwSize,Data))
			{
				VS_FIXEDFILEINFO *ffi;
				UINT Len;
				LPVOID lplpBuffer;
				if(VerQueryValue(Data,L"\\",&lplpBuffer,&Len))
				{
					ffi=(VS_FIXEDFILEINFO*)lplpBuffer;
					if(ffi->dwFileType==VFT_APP || ffi->dwFileType==VFT_DLL)
					{
						vi.Major=LOBYTE(HIWORD(ffi->dwFileVersionMS));
						vi.Minor=LOBYTE(LOWORD(ffi->dwFileVersionMS));
						vi.Revision=HIWORD(ffi->dwFileVersionLS);
						vi.Build=LOWORD(ffi->dwFileVersionLS);
						ret=true;
					}
				}
			}
			free(Data);
		}
	}
	return ret;
}

int WINAPI SortList(const void *el1, const void *el2, void * el3)
{
	struct ModuleInfo *Item1=(struct ModuleInfo *)el1, *Item2=(struct ModuleInfo *)el2;

	if (Item1->Flags&NEW)
	{
		if (!(Item2->Flags&NEW))
			return 1;
	}
	else
	{
		if (Item2->Flags&NEW)
			return -1;
	}
	return FSF.LStricmp(Item1->Title,Item2->Title);
}

DWORD GetInstallModulesInfo()
{
	DWORD Ret=S_NONE;
	FreeModulesInfo();

	size_t PluginsCount=Info.PluginsControl(INVALID_HANDLE_VALUE,PCTL_GETPLUGINS,0,0);
	HANDLE *Plugins=(HANDLE*)malloc(PluginsCount*sizeof(HANDLE));
	ipc.Modules=(ModuleInfo*)malloc((PluginsCount+1)*sizeof(ModuleInfo));

	if (ipc.Modules && Plugins)
	{
		ipc.CountModules=PluginsCount+1/*Far*/;

		for (size_t i=0,j=0; i<ipc.CountModules; i++)
		{
			memset(&ipc.Modules[i],0,sizeof(ModuleInfo));
			if (i==0) // Far
			{
				Info.AdvControl(&MainGuid,ACTL_GETFARMANAGERVERSION,0,&ipc.Modules[i].Version);
				wchar_t *Far=L"Far Manager", *FarAuthor=L"Eugene Roshal & Far Group";
				ipc.Modules[i].Guid=FarGuid;
				lstrcpy(ipc.Modules[i].Title,Far);
				lstrcpy(ipc.Modules[i].Description,Far);
				lstrcpy(ipc.Modules[i].Author,FarAuthor);
				GetModuleFileName(nullptr,ipc.Modules[i].ModuleName,MAX_PATH);
			}
			else // plugins
			{
				Info.PluginsControl(INVALID_HANDLE_VALUE,PCTL_GETPLUGINS,PluginsCount,Plugins);
				size_t size=Info.PluginsControl(Plugins[j],PCTL_GETPLUGININFORMATION,0,0);
				FarGetPluginInformation *FGPInfo=(FarGetPluginInformation*)malloc(size);
				if (FGPInfo)
				{
					FGPInfo->StructSize=sizeof(FarGetPluginInformation);
					Info.PluginsControl(Plugins[j++],PCTL_GETPLUGININFORMATION,size,FGPInfo);
					ipc.Modules[i].Guid=FGPInfo->GInfo->Guid;
					if (FGPInfo->Flags&FPF_ANSI) ipc.Modules[i].Flags|=ANSI;
					if (GetModuleHandle(FSF.PointToName(FGPInfo->ModuleName))) ipc.Modules[i].Flags|=ACTIVE;
					if (IsStdPlug(FGPInfo->GInfo->Guid)) ipc.Modules[i].Flags|=STD;
					VersionInfo CurFarVer={0,0,0,0};
					Info.AdvControl(&MainGuid,ACTL_GETFARMANAGERVERSION,0,&CurFarVer);
					if (FGPInfo->Flags&FPF_ANSI && !CheckFarVer(CurFarVer)) // �.�. ���� ������ 3410
						GetCurrentModuleVersion(FGPInfo->ModuleName,ipc.Modules[i].Version);
					else
						ipc.Modules[i].Version=FGPInfo->GInfo->Version;
					lstrcpyn(ipc.Modules[i].Title,FGPInfo->GInfo->Title,64);
					lstrcpyn(ipc.Modules[i].Description,FGPInfo->GInfo->Description,2048);
					lstrcpyn(ipc.Modules[i].Author,FGPInfo->GInfo->Author,MAX_PATH);
					lstrcpyn(ipc.Modules[i].ModuleName,FGPInfo->ModuleName,MAX_PATH);
					free(FGPInfo);
				}
				else
				{
					Ret=S_CANTGETINSTALLINFO;
					break;
				}
			}
			HANDLE h;
			if ((h=CreateFile(ipc.Modules[i].ModuleName,0,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr))!=INVALID_HANDLE_VALUE)
			{
				FILETIME Time;
				if (GetFileTime(h,0,0,&Time))
					GetStrFileTime(ipc.Modules[i].Date,&Time);
				CloseHandle(h);
			}
		}
		free(Plugins);
	}
	else Ret=S_CANTGETINSTALLINFO;

	if (Ret) FreeModulesInfo();
	return Ret;
}

#include "Download.cpp"

DWORD GetUpdatesLists()
{
	DWORD Ret=S_NONE;
	CreateDirectory(ipc.TempDirectory,nullptr);
	if (GetFileAttributes(ipc.TempDirectory)==INVALID_FILE_ATTRIBUTES)
		return S_CANTCREATTMP;

	wchar_t URL[1024];
	// far
	{
		lstrcpy(URL,ipc.opt.Stable?FarRemotePath1:FarRemotePath2);
		lstrcat(URL,FarUpdateFile);
		lstrcat(URL,phpRequest);
		if (!DownloadFile(FarRemoteSrv,URL,FarUpdateFile,false))
			Ret=S_CANTGETFARLIST;
	}
	// plug
	if (Ret==S_NONE)
	{
		lstrcpy(URL,PlugRemotePath1);
		lstrcat(URL,PlugUpdateFile);
		if (!DownloadFile(PlugRemoteSrv,URL,PlugUpdateFile,true))
			Ret=S_CANTGETPLUGLIST;
	}
	if (Ret)
	{
		DeleteFile(ipc.FarUpdateList);
		DeleteFile(ipc.PlugUpdateList);
	}
	return Ret;
}

#include "Undo.cpp"

void GetUpdModulesInfo()
{
	DWORD Ret;
	if (Ret=GetInstallModulesInfo())
	{
		SetStatus(Ret);
		return;
	}
	if (Ret=GetUpdatesLists())
	{
		SetStatus(Ret);
		return;
	}
	Ret=S_UPTODATE;
	CountUpdate=CountDownload=0;

	wchar_t *ListGuid=nullptr;
	PluginSettings settings(MainGuid, Info.SettingsControl);
	int len=lstrlen(settings.Get(0,L"Skip",L""));
	if (len>36)
	{
		ListGuid=(wchar_t*)malloc((len+1)*sizeof(wchar_t));
		if (ListGuid)
			settings.Get(0,L"Skip",ListGuid,len+1,L"");
	}

/**
[info]
version="1.0"

[far]
major="3"
minor="0"
build="3167"
platform="x86"
arc="Far30b3167.x86.20130209.7z"
msi="Far30b3167.x86.20130209.msi"
date="2013-02-09"
lastchange="t-rex 08.02.2013 16:52:35 +0200 - build 3167"
**/
	// far + ����������� �����
	{
		wchar_t Buf[MAX_PATH];
		GetPrivateProfileString(L"info",L"version",L"",Buf,ARRAYSIZE(Buf),ipc.FarUpdateList);
		if (!lstrcmp(Buf, L"1.0"))
		{
			LPCWSTR Section=L"far";
			ipc.Modules[0].New.Version.Major=GetPrivateProfileInt(Section,L"major",-1,ipc.FarUpdateList);
			ipc.Modules[0].New.Version.Minor=GetPrivateProfileInt(Section,L"minor",-1,ipc.FarUpdateList);
			ipc.Modules[0].New.Version.Build=GetPrivateProfileInt(Section,L"build",-1,ipc.FarUpdateList);
			ipc.Modules[0].New.MinFarVersion=ipc.Modules[0].New.Version;
			GetPrivateProfileString(Section,L"date",L"",ipc.Modules[0].New.Date,ARRAYSIZE(ipc.Modules[0].New.Date),ipc.FarUpdateList);
			GetPrivateProfileString(Section,L"arc",L"",ipc.Modules[0].New.ArcName,ARRAYSIZE(ipc.Modules[0].New.ArcName),ipc.FarUpdateList);
			// ���� �������� ��� ������ � ������ �����...
			if (ipc.Modules[0].New.ArcName[0])
			{
				lstrcpy(ipc.Modules[0].Changelog,StdPlug[0].Changelog);
				ipc.Modules[0].Flags|=INFO;

				if (CmpListGuid(ListGuid,(GUID&)FarGuid))
					ipc.Modules[0].Flags|=SKIP;
				else if (NeedUpdate(ipc.Modules[0].Version,ipc.Modules[0].New.Version) || FSF.LStricmp(ipc.Modules[0].Date,ipc.Modules[0].New.Date)<0)
				{
					ipc.Modules[0].Flags|=UPD;
					CountUpdate++;
					Ret=S_UPDATE;
				}
				// �������
				for (size_t i=1; i<ipc.CountModules; i++)
				{
					if (ipc.Modules[i].Flags&STD)
					{
						if (!(ipc.Modules[0].Flags&UPD))
						{
							ipc.Modules[i].Flags|=INFO;
							ipc.Modules[i].New.Version=ipc.Modules[i].Version;
							lstrcpy(ipc.Modules[i].New.Date,ipc.Modules[0].Date);
						}
						else
							lstrcpy(ipc.Modules[i].New.Date,ipc.Modules[0].New.Date);
						// �������� ���
						for (size_t j=1; j<=17; j++)
						{
							if (*StdPlug[j].Guid==ipc.Modules[i].Guid)
							{
								if (j<16)
								{
									wchar_t url[512]=L"http://farmanager.googlecode.com/svn/trunk/plugins/";
									lstrcat(url,StdPlug[j].Changelog);
									lstrcat(url,L"/changelog");
									lstrcpy(ipc.Modules[i].Changelog,url);
								}
								else
									lstrcpy(ipc.Modules[i].Changelog,StdPlug[j].Changelog);
							}
						}
					}
				}
			}
		}
		else
			Ret=S_CANTGETFARUPDINFO;
	}
/**
<?xml version="1.0" encoding="UTF-8"?>
<plugring>
<plugins>
<plugin plugin id="697" uid="9F25A250-45D2-45a0-90A3-5686B2A048FA" title="PicView Advanced"/>
<files>
<file id="960" plugin_id="697" flags="75" filename="PicViewAdvW_7.rar" version_major="2" version_minor="0" version_build="7" version_revision="0"
 far_major="2" far_minor="0" far_build="0" downloaded_count="213" date_added="2012-11-15 10:59:45">
</file>
<file id="1084" plugin_id="697" flags="73" filename="PicViewAdvW.x86_8.rar" version_major="3" version_minor="0" version_build="8" version_revision="0"
 far_major="3" far_minor="0" far_build="2927" downloaded_count="545" date_added="2012-12-31 11:41:38" >
</file>
<file id="1085" plugin_id="697" flags="81" filename="PicViewAdvW.x64_8.rar" version_major="3" version_minor="0" version_build="8" version_revision="0"
 far_major="3" far_minor="0" far_build="2927" downloaded_count="554" date_added="2012-12-31 11:42:31" >
</file>
</files>
</plugin>
</plugins>
**/
	// ��������� �����
	if (Ret!=S_CANTGETFARUPDINFO)
	{
		TiXmlDocument doc;
		char path[MAX_PATH];
		WideCharToMultiByte(CP_ACP,0,ipc.PlugUpdateList,-1,path,MAX_PATH,nullptr,nullptr);
		if (doc.LoadFile(path))
		{
			TiXmlElement *plugring=doc.FirstChildElement("plugring");
			if (plugring)
			{
				const TiXmlHandle root(plugring);
				for (const TiXmlElement *plugins=root.FirstChildElement("plugins").Element(); plugins; plugins=plugins->NextSiblingElement("plugins"))
				{
					const TiXmlElement *plug=plugins->FirstChildElement("plugin");
					if (plug)
					{
						ModuleInfo *CurInfo=nullptr;
						GUID plugGUID;
						wchar_t *Buf=CharToWChar(plug->Attribute("uid"));
						if (Buf)
						{
							if (StrToGuid(Buf,plugGUID) && !IsStdPlug(plugGUID))
							{
								for (size_t i=1; i<ipc.CountModules; i++)
								{
									if (ipc.Modules[i].Guid==plugGUID)
									{
										CurInfo=&ipc.Modules[i];
										break;
									}
								}
								if (ipc.opt.Mode==MODE_NEW && !CurInfo) // ������ �����
								{
									ModuleInfo *NewInfo=(ModuleInfo *)realloc(ipc.Modules,(ipc.CountModules+1)*sizeof(ModuleInfo));
									if (NewInfo)
									{
										ipc.Modules=NewInfo;
										memset(&ipc.Modules[ipc.CountModules],0,sizeof(ModuleInfo));
										CurInfo=&ipc.Modules[ipc.CountModules];
										ipc.CountModules++;
										CurInfo->Guid=plugGUID;
										CurInfo->Flags|=NEW;
									}
								}
							}
							free(Buf); Buf=nullptr;
						}
						if (CurInfo)
						{
							wchar_t *Buf=nullptr;
							if (ipc.opt.Mode==MODE_NEW)
							{
								Buf=CharToWChar(plug->Attribute("title"));
								if (Buf)
								{
									lstrcpyn(CurInfo->Title,Buf,64);
									free(Buf); Buf=nullptr;
								}
								Buf=CharToWChar(plug->Attribute("description"));
								if (Buf)
								{
									lstrcpyn(CurInfo->Description,Buf,2048);
									ReplaseSymbol(CurInfo->Description);
									free(Buf); Buf=nullptr;
								}
							}

							Buf=CharToWChar(plug->Attribute("id"));
							if (Buf)
							{
								lstrcpy(CurInfo->pid,Buf);
								free(Buf); Buf=nullptr;
							}
							Buf=CharToWChar(plug->Attribute("forum_url"));
							if (Buf)
							{
								if (Buf[0])
								{
									lstrcpyn(CurInfo->Changelog,Buf,2048);
									if (StrStr(CurInfo->Changelog,L"forum.farmanager.com"))
										lstrcat(CurInfo->Changelog,L"&start=100000"); // ���� ����� ��� �������� � ����� ������ ������ :)
								}
								free(Buf); Buf=nullptr;
							}
							if (const TiXmlElement *filesElem=plug->FirstChildElement("files"))
							{
								for (const TiXmlElement *file=filesElem->FirstChildElement("file"); file; file=file->NextSiblingElement("file"))
								{
									Buf=CharToWChar(file->Attribute("flags"));
									if (Buf)
									{
										enum FILE_FLAG {
											BINARY = 1,
											X86 = 8,
											X64 = 16
										};
										DWORD flag=StringToNumber(Buf,flag);
										free(Buf); Buf=nullptr;
										if ((flag&BINARY) &&
#ifdef _WIN64
												(flag&X64)
#else
												(flag&X86)
#endif
										)
										{
											VersionInfo MinFarVer=MAKEFARVERSION(MIN_FAR_MAJOR_VER,MIN_FAR_MINOR_VER,0,MIN_FAR_BUILD,VS_RELEASE);
											VersionInfo CurFarVer={0,0,0,0};
											VersionInfo CurVer={0,0,0,0};

											// ����������� CurFarVer
											Buf=CharToWChar(file->Attribute("far_major"));
											if (Buf)
											{
												CurFarVer.Major=StringToNumber(Buf,CurFarVer.Major);
												free(Buf); Buf=nullptr;
											}
											Buf=CharToWChar(file->Attribute("far_minor"));
											if (Buf)
											{
												CurFarVer.Minor=StringToNumber(Buf,CurFarVer.Minor);
												free(Buf); Buf=nullptr;
											}
											Buf=CharToWChar(file->Attribute("far_build"));
											if (Buf)
											{
												CurFarVer.Build=StringToNumber(Buf,CurFarVer.Build);
												free(Buf); Buf=nullptr;
											}

											if (NeedUpdate(MinFarVer,CurFarVer,true) || ((CurInfo->Flags&ANSI) && CheckFarVer(CurFarVer,true)))
											{
												// ����������� CurVer
												Buf=CharToWChar(file->Attribute("version_major"));
												if (Buf)
												{
													CurVer.Major=StringToNumber(Buf,CurVer.Major);
													free(Buf); Buf=nullptr;
												}
												Buf=CharToWChar(file->Attribute("version_minor"));
												if (Buf)
												{
													CurVer.Minor=StringToNumber(Buf,CurVer.Minor);
													free(Buf); Buf=nullptr;
												}
												Buf=CharToWChar(file->Attribute("version_revision"));
												if (Buf)
												{
													CurVer.Revision=StringToNumber(Buf,CurVer.Revision);
													free(Buf); Buf=nullptr;
												}
												Buf=CharToWChar(file->Attribute("version_build"));
												if (Buf)
												{
													CurVer.Build=StringToNumber(Buf,CurVer.Build);
													free(Buf); Buf=nullptr;
												}

												if (NeedUpdate(CurInfo->New.Version,CurVer,true))
												{
													CurInfo->New.MinFarVersion=CurFarVer;
													CurInfo->New.Version=CurVer;

													Buf=CharToWChar(file->Attribute("id"));
													if (Buf)
													{
														lstrcpy(CurInfo->fid,Buf);
														free(Buf); Buf=nullptr;
													}
													Buf=CharToWChar(file->Attribute("date_added"));
													if (Buf)
													{
														lstrcpyn(CurInfo->New.Date,Buf,11);
														free(Buf); Buf=nullptr;
													}
													Buf=CharToWChar(file->Attribute("downloaded_count"));
													if (Buf)
													{
														lstrcpy(CurInfo->Downloads,Buf);
														free(Buf); Buf=nullptr;
													}
													Buf=CharToWChar(file->Attribute("filename"));
													if (Buf)
													{
														lstrcpy(CurInfo->New.ArcName,Buf);
														free(Buf); Buf=nullptr;
													}
													// ���� �������� ��� ������..
													if (CurInfo->New.ArcName[0])
													{
														CurInfo->Flags|=INFO;
														if (CurInfo->Flags&NEW)
															GetNewModuleDir(CurInfo->New.ArcName,CurInfo->ModuleName);
														if (CmpListGuid(ListGuid,CurInfo->Guid))
															CurInfo->Flags|=SKIP;
														else if (!(CurInfo->Flags&NEW) && NeedUpdate(CurInfo->Version,CurInfo->New.Version))
														{
															CurInfo->Flags|=UPD;
															CountUpdate++;
															Ret=S_UPDATE;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		if (Ret!=S_UPTODATE && Ret!=S_UPDATE)
			Ret=S_CANTGETPLUGUPDINFO;
	}
	SetStatus(Ret);
	ReadCache();
	// ���������
	if ((Ret==S_UPTODATE || Ret==S_UPDATE))
		FSF.qsort(&ipc.Modules[1],ipc.CountModules-1,sizeof(ipc.Modules[1]),SortList,nullptr);

	if (ListGuid) free(ListGuid);
	DeleteFile(ipc.FarUpdateList);
	DeleteFile(ipc.PlugUpdateList);
	return;
}

enum {
	DlgBORDER = 0,  // 0
	DlgLIST,        // 1
	DlgSEP1,        // 2
	DlgDESC,        // 3
	DlgAUTHOR,      // 4
	DlgPATH,        // 5
	DlgGUID,        // 6
	DlgINFO,        // 7
	DlgSEP2,        // 8
	DlgUPD,         // 9
	DlgCANCEL,      // 10
};

void MakeListItem(ModuleInfo *Cur, wchar_t *Buf, struct FarListItem &Item, DWORD Percent=-1)
{
	wchar_t Ver[64], NewVer[64], Date[11], NewDate[11], Status[80];

	if ((Cur->Flags&STD) || (ipc.opt.Mode!=MODE_UNDO && !(Cur->Flags&INFO)) || (ipc.opt.Mode!=MODE_UNDO && GetStatus()==S_COMPLET && !(Cur->Flags&ARC)))
		Item.Flags|=LIF_GRAYED;
	if (Cur->Flags&UPD)
	{
		Item.Flags|=(LIF_CHECKED|0x2b);
	}
	else if (Cur->Flags&SKIP)
		Item.Flags|=(LIF_CHECKED|0x2d);

	if (Cur->Flags&NEW) Ver[0]=0;
	else FSF.sprintf(Ver,L"%d.%d.%d.%d",Cur->Version.Major,Cur->Version.Minor,Cur->Version.Revision,Cur->Version.Build);

	if (ipc.opt.Mode!=MODE_UNDO && (Cur->Flags&INFO)) FSF.sprintf(NewVer,L"%d.%d.%d.%d",Cur->New.Version.Major,Cur->New.Version.Minor,Cur->New.Version.Revision,Cur->New.Version.Build);
	else if (ipc.opt.Mode==MODE_UNDO && (Cur->Flags&UNDO)) FSF.sprintf(NewVer,L"%d.%d.%d.%d",Cur->Undo.Version.Major,Cur->Undo.Version.Minor,Cur->Undo.Version.Revision,Cur->Undo.Version.Build);
	else NewVer[0]=0;

	if (ipc.opt.Date)
	{
		lstrcpy(Date,Cur->Date);
		lstrcpy(NewDate,(ipc.opt.Mode!=MODE_UNDO?Cur->New.Date:Cur->Undo.Date));
	}
	else
	{
		CopyReverseTime(Date,Cur->Date);
		CopyReverseTime(NewDate,(ipc.opt.Mode!=MODE_UNDO?Cur->New.Date:Cur->Undo.Date));
	}

	if (Cur->Flags&ERR) lstrcpy(Status,MSG(MError));
	else if (Cur->Flags&ARC) lstrcpy(Status,MSG(MLoaded));
	else if (Percent!=-1) FSF.sprintf(Status,L"%3d%%",Percent);
	else Status[0]=0;

	if (ipc.opt.Mode==MODE_NEW)
		FSF.sprintf(Buf,L"%s%-35.35s%c%c%c%-14.14s%c%10.10s%c%7.7s",L"  ",Cur->Title,ipc.opt.ShowDraw?0x2502:L' ',
										Cur->Flags&UPD?0x2192:L' ',ipc.opt.ShowDraw?0x2502:L' ',NewVer,ipc.opt.ShowDraw?0x2502:L' ',
										NewDate,ipc.opt.ShowDraw?0x2502:L' ',Status);
	else if (!ipc.opt.ShowDate)
		FSF.sprintf(Buf,L"%c%c%-22.22s%c%-14.14s%c%c%c%-14.14s%c%10.10s%c%7.7s",
										Cur->Flags&STD?L'S':(Cur->Flags&ANSI?L'A':L' '),Cur->Flags&ACTIVE?0x2022:L' ',Cur->Title,ipc.opt.ShowDraw?0x2502:L' ',
										Ver,ipc.opt.ShowDraw?0x2502:L' ',Cur->Flags&UPD?0x2192:L' ',ipc.opt.ShowDraw?0x2502:L' ',NewVer,ipc.opt.ShowDraw?0x2502:L' ',
										NewDate,ipc.opt.ShowDraw?0x2502:L' ',Status);
	else
		FSF.sprintf(Buf,L"%c%c%-15.15s%c%-12.12s%c%10.10s%c%c%c%-12.12s%c%10.10s%c%7.7s",
										Cur->Flags&STD?L'S':(Cur->Flags&ANSI?L'A':L' '),Cur->Flags&ACTIVE?0x2022:L' ',Cur->Title,ipc.opt.ShowDraw?0x2502:L' ',
										Ver,ipc.opt.ShowDraw?0x2502:L' ',Date,ipc.opt.ShowDraw?0x2502:L' ',Cur->Flags&UPD?0x2192:L' ',ipc.opt.ShowDraw?0x2502:L' ',
										NewVer,ipc.opt.ShowDraw?0x2502:L' ',NewDate,ipc.opt.ShowDraw?0x2502:L' ',Status);
	Item.Text=Buf;
}

void MakeListItemInfo(HANDLE hDlg,void *Pos)
{
	ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,Pos);
	ModuleInfo *Cur=Tmp?*Tmp:nullptr;
	if (Cur)
	{
		wchar_t Buf[MAX_PATH];
		int len=lstrlen(Cur->Description);
		if (len<=(80-2-2))
		{
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgDESC,Cur->Description);
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgAUTHOR,L"");
		}
		else if (ipc.opt.Mode==MODE_NEW)
		{
			lstrcpyn(Buf,Cur->Description,76+1);
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgDESC,Buf);
			if (len-76>76)
			{
				lstrcpyn(Buf,Cur->Description+76,76-3+1);
				lstrcat(Buf,L"...");
			}
			else
				lstrcpyn(Buf,Cur->Description+76,76+1);
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgAUTHOR,Buf);
		}
		else
		{
			lstrcpyn(Buf,Cur->Description,76-3+1);
			lstrcat(Buf,L"...");
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgDESC,Buf);
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgAUTHOR,L"");
		}

		len=lstrlen(Cur->Author);
		if (ipc.opt.Mode!=MODE_NEW)
		{
			if (len<=(80-2-2))
				Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgAUTHOR,Cur->Author);
			else
			{
				lstrcpyn(Buf,Cur->Author,76-3+1);
				lstrcat(Buf,L"...");
				Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgAUTHOR,Buf);
			}
		}

		lstrcpy(Buf,Cur->ModuleName);
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgPATH,FSF.TruncPathStr(Buf,80-2-2));

		GuidToStr(Cur->Guid,Buf);
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgGUID,Buf);

		if (Cur->Flags&ERR)
			lstrcpy(Buf,MSG(MCantDownloadArc));
		else
		{
			if (Cur->Flags&STD)
			{
				if (ipc.Modules[0].Flags&INFO) FSF.sprintf(Buf,L"%s \"%s\"",MSG(MStandard),ipc.Modules[0].New.ArcName);
				else Buf[0]=0;
			}
			else
			{
				if (ipc.opt.Mode!=MODE_UNDO && (Cur->Flags&INFO)) FSF.sprintf(Buf,L"<Far %d.%d.%d, downloads %s> \"%s\"",Cur->New.MinFarVersion.Major,Cur->New.MinFarVersion.Minor,Cur->New.MinFarVersion.Build,Cur->Downloads[0]?Cur->Downloads:L"0",Cur->New.ArcName);
				else if (ipc.opt.Mode==MODE_UNDO && (Cur->Flags&UNDO)) FSF.sprintf(Buf,L"<Far %d.%d.%d> \"%s\"",Cur->Undo.MinFarVersion.Major,Cur->Undo.MinFarVersion.Minor,Cur->Undo.MinFarVersion.Build,Cur->Undo.ArcName);
				else lstrcpy(Buf,MSG(MIsNonInfo));
			}
		}
		if (lstrlen(Buf)>(80-2-2))
		{
			wchar_t Buf2[80];
			lstrcpyn(Buf2,Buf,80-2-2-3+1);
			lstrcat(Buf2,L"...");
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgINFO,Buf2);
		}
		else
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgINFO,Buf);
	}
	else
	{
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgDESC,L"");
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgAUTHOR,L"");
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgPATH,L"");
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgGUID,L"");
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgINFO,(void*)MSG(MIsNonInfo));
	}
	Info.SendDlgMessage(hDlg,DM_REDRAW,0,0);
}

bool MakeList(HANDLE hDlg,intptr_t SetCurPos=0)
{
	CountListItems=0;
	struct FarListInfo ListInfo={sizeof(FarListInfo)};
	Info.SendDlgMessage(hDlg,DM_LISTINFO,DlgLIST,&ListInfo);
	if (ListInfo.ItemsNumber)
		Info.SendDlgMessage(hDlg,DM_LISTDELETE,DlgLIST,0);

	for (size_t i=0,ii=0; i<ipc.CountModules; i++)
	{
		ModuleInfo *Cur=&ipc.Modules[i];
		if (ipc.opt.Mode==MODE_NEW && !(Cur->Flags&NEW))
			continue;
		if (ipc.opt.Mode==MODE_UNDO && !(Cur->Flags&UNDO))
			continue;
		if ((GetStatus()!=S_COMPLET&&((Cur->Flags&INFO)&&!(Cur->Flags&STD)))||(GetStatus()==S_COMPLET&&(Cur->Flags&ARC))||ipc.opt.ShowDisable||ipc.opt.Mode==MODE_UNDO)
		{
			wchar_t Buf[MAX_PATH];
			struct FarListItem Item={};
			MakeListItem(Cur,Buf,Item);
			if (!SetCurPos && ii==0)
				Item.Flags|=LIF_SELECTED;
			struct FarList List={sizeof(FarList)};
			List.ItemsNumber=1;
			List.Items=&Item;

			// ���� ������ �������� �������...
			if (Info.SendDlgMessage(hDlg,DM_LISTADD,DlgLIST,&List))
			{
				Cur->ID=(DWORD)ii;
				// ... �� ����������� ������ � ��������� �����
				struct FarListItemData Data={sizeof(FarListItemData)};
				Data.Index=ii++;
				CountListItems=ii;
				Data.DataSize=sizeof(Cur);
				Data.Data=&Cur;
				Info.SendDlgMessage(hDlg,DM_LISTSETDATA,DlgLIST,&Data);
			}
		}
	}
	if (SetCurPos)
	{
		FarListPos ListPos={sizeof(FarListPos)};
		ListPos.SelectPos=SetCurPos>0?SetCurPos:ListInfo.SelectPos;
		ListPos.TopPos=ListInfo.TopPos;
		Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,DlgLIST,&ListPos);
	}
	wchar_t Buf[64];
	if (CountUpdate)
		FSF.sprintf(Buf,MSG(MSepInfo2),CountUpdate,CountDownload);
	else
		FSF.sprintf(Buf,MSG(MSepInfo),CountListItems);
	Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgSEP2,Buf);
	return true;
}

bool isMainDlg()
{
	struct WindowType Type={sizeof(WindowType)};
	if (Info.AdvControl(&MainGuid,ACTL_GETWINDOWTYPE,0,&Type) && Type.Type==WTYPE_DIALOG)
	{
		struct DialogInfo DInfo={sizeof(DialogInfo)};
		if (hDlg && Info.SendDlgMessage(hDlg,DM_GETDIALOGINFO,0,&DInfo) && ModulesDlgGuid==DInfo.Id)
			return true;
	}
	return false;
}

BOOL CALLBACK DownloadProcEx(ModuleInfo *CurInfo,DWORD Percent)
{
	if (Percent<100)
	{
		static DWORD dwTicks;
		DWORD dwNewTicks = GetTickCount();
		if (dwNewTicks - dwTicks < 500)
			return false;
		dwTicks = dwNewTicks;
	}
	if (isMainDlg())
	{
		wchar_t Buf[MAX_PATH];
		struct FarListItem Item={};
		MakeListItem(CurInfo,Buf,Item,Percent);
		intptr_t CurPos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,nullptr);
		if (CurInfo->ID==CurPos)
			Item.Flags|=LIF_SELECTED;
		struct FarListUpdate FLU={sizeof(FarListUpdate),CurInfo->ID,Item};
		if (!Info.SendDlgMessage(hDlg,DM_LISTUPDATE,DlgLIST,&FLU))
			return FALSE;
	}
	return TRUE;
}

bool DownloadUpdates()
{
	wchar_t URL[MAX_PATH], LocalFile[MAX_PATH];
	bool bUPD=false;
	NeedRestart=false;
	for(size_t i=0; i<ipc.CountModules; i++)
	{
		if (ipc.Modules[i].Flags&UPD)
		{
			// url ������ ��� �������
			lstrcpy(URL,i==0?(ipc.opt.Stable?FarRemotePath1:FarRemotePath2):PlugRemotePath2);
			lstrcat(URL,i==0?ipc.Modules[i].New.ArcName:ipc.Modules[i].fid);
			// ���� �������� �����
			lstrcpy(LocalFile,ipc.TempDirectory);
			lstrcat(LocalFile,ipc.Modules[i].New.ArcName);

			struct DownloadParam Param={&ipc.Modules[i],DownloadProcEx};
			if (WinInetDownloadEx(i==0?FarRemoteSrv:PlugRemoteSrv,URL,LocalFile,false,&Param)==0)
			{
				CountDownload++;
				wchar_t Buf[64];
				FSF.sprintf(Buf,MSG(MSepInfo2),CountUpdate,CountDownload);
				if (isMainDlg())
					Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgSEP2,Buf);
				NeedRestart=true;
			}
		}
		// ��������
		if (GetStatus()==S_NONE)
		{
			NeedRestart=false;
			return false;
		}
		if (ipc.Modules[i].Flags&INFO)
			bUPD=true; // ���-�� ����, �� �� ��������
	}
	// ���� ���-�� �������
	if (NeedRestart)
	{
		SetStatus(S_COMPLET);
	}
	else
	{
		// ����� ����������� ������
		if (bUPD)
			SetStatus(S_UPTODATE);
	}
	if (isMainDlg())
		Info.SendDlgMessage(hDlg,DN_UPDDLG,0,0);
	return true;
}

#include "SendModules.cpp"
#include "GetNewModules.cpp"


intptr_t WINAPI ShowModulesDialogProc(HANDLE hDlg,intptr_t Msg,intptr_t Param1,void *Param2)
{
	switch(Msg)
	{
		case DN_INITDIALOG:
		{
			if (GetStatus()==S_COMPLET)
				Info.SendDlgMessage(hDlg,DN_UPDDLG,0,0);
			else
			{
				MakeList(hDlg);
				MakeListItemInfo(hDlg,0);
			}
			Info.SendDlgMessage(hDlg,DM_SETMOUSEEVENTNOTIFY,1,0);
			break;
		}

	/************************************************************************/

		case DN_RESIZECONSOLE:
		{
			COORD c={-1,-1};
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,false,0);
			Info.SendDlgMessage(hDlg,DM_MOVEDIALOG,1,&c);
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,true,0);
			return true;
		}

		/************************************************************************/

		case DN_LISTCHANGE:
			if (Param1==DlgLIST)
			{
				MakeListItemInfo(hDlg,Param2);
				return true;
			}
			break;

	/************************************************************************/

		case DN_CTLCOLORDLGITEM:
			if (Param1==DlgSEP2 && CountUpdate)
			{
				FarColor Color;
				struct FarDialogItemColors *Colors=(FarDialogItemColors*)Param2;
				Info.AdvControl(&MainGuid,ACTL_GETCOLOR,COL_DIALOGHIGHLIGHTSELECTEDBUTTON,&Color);
				Colors->Colors[0]=Color;
			}
			break;

	/************************************************************************/

		case DN_INPUT:
		{
			const INPUT_RECORD* record=(const INPUT_RECORD *)Param2;

			if (record->EventType==MOUSE_EVENT)
			{
				// ���������� ������ ����
				if ((record->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED || record->Event.MouseEvent.dwButtonState==RIGHTMOST_BUTTON_PRESSED)
					 && record->Event.MouseEvent.dwEventFlags!=DOUBLE_CLICK)
				{
					SMALL_RECT dlgRect;
					Info.SendDlgMessage(hDlg,DM_GETDLGRECT,0,&dlgRect);
					// �������� � LIST�
					if (record->Event.MouseEvent.dwMousePosition.X>dlgRect.Left && record->Event.MouseEvent.dwMousePosition.X<dlgRect.Left+4
					&& record->Event.MouseEvent.dwMousePosition.Y>dlgRect.Top && record->Event.MouseEvent.dwMousePosition.Y<dlgRect.Bottom-7)
					{
						FarListPos ListPos={sizeof(FarListPos)};
						Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,&ListPos);
						intptr_t OldPos=ListPos.SelectPos;
						ListPos.SelectPos=ListPos.TopPos+(record->Event.MouseEvent.dwMousePosition.Y-1-dlgRect.Top);
						intptr_t NewPos=Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,DlgLIST,&ListPos);
						// ��� ���, ���� ��� �������
						if (GetStatus()!=S_DOWNLOAD && NewPos==ListPos.SelectPos && record->Event.MouseEvent.dwMousePosition.X>=dlgRect.Left+1 && record->Event.MouseEvent.dwMousePosition.X<=dlgRect.Left+3)
						{
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)ListPos.SelectPos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
							{
								struct FarListGetItem FLGI={sizeof(FarListGetItem)};
								FLGI.ItemIndex=ListPos.SelectPos;
								if (Info.SendDlgMessage(hDlg,DM_LISTGETITEM,DlgLIST,&FLGI))
								{
									if (!(FLGI.Item.Flags&LIF_GRAYED))
									{
										if (record->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED)
										{
											if (LOWORD(FLGI.Item.Flags)==0x2b) FLGI.Item.Flags&= ~(LIF_CHECKED|0x2b);
											else { FLGI.Item.Flags&= ~(LIF_CHECKED|0x2d); FLGI.Item.Flags|=(LIF_CHECKED|0x2b); }
										}
										else
										{
											if (LOWORD(FLGI.Item.Flags)==0x2d) FLGI.Item.Flags&= ~(LIF_CHECKED|0x2d);
											else { FLGI.Item.Flags&= ~(LIF_CHECKED|0x2b); FLGI.Item.Flags|=(LIF_CHECKED|0x2d); }
										}
										struct FarListUpdate FLU={sizeof(FarListUpdate)};
										FLU.Index=FLGI.ItemIndex;
										FLU.Item=FLGI.Item;
										if (Info.SendDlgMessage(hDlg,DM_LISTUPDATE,DlgLIST,&FLU))
										{
											if (record->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED)
											{
												if (Cur->Flags&UPD) { Cur->Flags&=~UPD; CountUpdate--; }
												else { Cur->Flags|=UPD; Cur->Flags&=~SKIP; Cur->Flags&=~ERR; CountUpdate++; }
											}
											else
											{
												if (Cur->Flags&SKIP) Cur->Flags&=~SKIP;
												else { Cur->Flags|=SKIP; Cur->Flags&=~UPD; Cur->Flags&=~ERR; CountUpdate?CountUpdate--:0; }
											}
											wchar_t Buf[64];
											if (CountUpdate)
												FSF.sprintf(Buf,MSG(MSepInfo2),CountUpdate,CountDownload);
											else
												FSF.sprintf(Buf,MSG(MSepInfo),CountListItems);
											Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgSEP2,Buf);
											return false;
										}
									}
								}
							}
						}
					}
				}
			}
			return true;
		}

		/************************************************************************/

		case DN_CONTROLINPUT:
		{
			const INPUT_RECORD* record=(const INPUT_RECORD *)Param2;
			if (record->EventType==MOUSE_EVENT)
			{
				if (Param1==DlgLIST && record->Event.MouseEvent.dwButtonState==RIGHTMOST_BUTTON_PRESSED &&
						record->Event.MouseEvent.dwEventFlags==DOUBLE_CLICK)
					goto GOTO_CTRLH;
				else if (Param1==DlgLIST && record->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
						record->Event.MouseEvent.dwEventFlags==DOUBLE_CLICK)
					goto GOTO_F2;
				else if ((Param1==DlgDESC || ipc.opt.Mode==MODE_NEW&&Param1==DlgAUTHOR) && record->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
						record->Event.MouseEvent.dwEventFlags==DOUBLE_CLICK)
					goto GOTO_F3;
				else if (Param1==DlgPATH && record->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
						record->Event.MouseEvent.dwEventFlags==DOUBLE_CLICK)
					goto GOTO_F4;
				else if (Param1==DlgSEP1 && record->Event.MouseEvent.dwButtonState==RIGHTMOST_BUTTON_PRESSED &&
						record->Event.MouseEvent.dwEventFlags==DOUBLE_CLICK)
					goto GOTO_F6;
				else if (Param1==DlgSEP1 && record->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
						record->Event.MouseEvent.dwEventFlags==DOUBLE_CLICK)
					goto GOTO_F7;
				else if (Param1==DlgPATH && record->Event.MouseEvent.dwButtonState==RIGHTMOST_BUTTON_PRESSED &&
						record->Event.MouseEvent.dwEventFlags==DOUBLE_CLICK)
					goto GOTO_F8;
				else if (Param1==DlgINFO && record->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
						record->Event.MouseEvent.dwEventFlags==DOUBLE_CLICK)
				{
					intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
					ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
					ModuleInfo *Cur=Tmp?*Tmp:nullptr;
					if (Cur)
					{
						wchar_t url[2048], Guid[37];
						GuidToStr(Cur->Guid,Guid);
						GetPrivateProfileString(Guid,L"URLHome",L"",url,ARRAYSIZE(url),ipc.Config);
						if (url[0]==0)
						{
							wchar_t local_config[MAX_PATH];
							GetModuleDir(Cur->ModuleName,local_config);
							// ���������
							{
								int len=lstrlen(local_config);
								if (len>5 && !lstrcmpi(&local_config[len-5],L"\\bin\\"))
									local_config[len-4]=0;
							}
							lstrcat(local_config,L"UpdateEx.dll.local.config");
							GetPrivateProfileString(Guid,L"URLHome",L"",url,ARRAYSIZE(url),local_config);
						}
						if (url[0])
						{
							ShellExecute(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
							return true;
						}
						else if (Cur->Guid==FarGuid || (Cur->Flags&STD))
						{
							ShellExecute(nullptr,L"open",L"http://www.farmanager.com/nightly.php",nullptr,nullptr,SW_SHOWNORMAL);
							return true;
						}
						else if (Cur->pid[0])
						{
							lstrcpy(url,L"http://plugring.farmanager.com/plugin.php?pid=");
							lstrcat(url,Cur->pid);
							ShellExecute(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
							return true;
						}
						else
							MessageBeep(MB_OK);
					}
				}
				return false;
			}
			else if (record->EventType==KEY_EVENT && record->Event.KeyEvent.bKeyDown)
			{
				WORD vk=record->Event.KeyEvent.wVirtualKeyCode;
				if (IsNone(record))
				{
					if (Param1==DlgLIST)
					{
						if (vk==VK_INSERT || vk==VK_DELETE || vk==VK_ADD || vk==VK_SUBTRACT)
						{
							if (GetStatus()!=S_DOWNLOAD)
							{
								struct FarListPos FLP={sizeof(FarListPos)};
								Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,&FLP);
								ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)FLP.SelectPos);
								ModuleInfo *Cur=Tmp?*Tmp:nullptr;
								if (Cur)
								{
									struct FarListGetItem FLGI={sizeof(FarListGetItem)};
									FLGI.ItemIndex=FLP.SelectPos;
									if (Info.SendDlgMessage(hDlg,DM_LISTGETITEM,DlgLIST,&FLGI))
									{
										if (!(FLGI.Item.Flags&LIF_GRAYED))
										{
											if (vk==VK_INSERT || vk==VK_ADD)
											{
												if (LOWORD(FLGI.Item.Flags)==0x2b) FLGI.Item.Flags&= ~(LIF_CHECKED|0x2b);
												else { FLGI.Item.Flags&= ~(LIF_CHECKED|0x2d); FLGI.Item.Flags|=(LIF_CHECKED|0x2b); }
											}
											else
											{
												if (LOWORD(FLGI.Item.Flags)==0x2d) FLGI.Item.Flags&= ~(LIF_CHECKED|0x2d);
												else { FLGI.Item.Flags&= ~(LIF_CHECKED|0x2b); FLGI.Item.Flags|=(LIF_CHECKED|0x2d); }
											}
											struct FarListUpdate FLU={sizeof(FarListUpdate)};
											FLU.Index=FLGI.ItemIndex;
											FLU.Item=FLGI.Item;
											if (Info.SendDlgMessage(hDlg,DM_LISTUPDATE,DlgLIST,&FLU))
											{
												if (vk==VK_INSERT || vk==VK_ADD)
												{
													if (Cur->Flags&UPD) { Cur->Flags&=~UPD; CountUpdate--; }
													else { Cur->Flags|=UPD; Cur->Flags&=~SKIP; Cur->Flags&=~ERR; CountUpdate++; }
												}
												else
												{
													if (Cur->Flags&SKIP) Cur->Flags&=~SKIP;
													else { Cur->Flags|=SKIP; Cur->Flags&=~UPD; Cur->Flags&=~ERR; CountUpdate?CountUpdate--:0; }
												}
												FLP.SelectPos++;
												Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,DlgLIST,&FLP);
												wchar_t Buf[64];
												if (CountUpdate)
													FSF.sprintf(Buf,MSG(MSepInfo2),CountUpdate,CountDownload);
												else
													FSF.sprintf(Buf,MSG(MSepInfo),CountListItems);
												Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgSEP2,Buf);
												return true;
											}
										}
									}
								}
							}
							MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_F2)
						{
GOTO_F2:
							intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
							{
								wchar_t url[2048], Guid[37];
								GuidToStr(Cur->Guid,Guid);
								GetPrivateProfileString(Guid,L"URLChangelog",L"",url,ARRAYSIZE(url),ipc.Config);
								if (url[0]==0)
								{
									wchar_t local_config[MAX_PATH];
									GetModuleDir(Cur->ModuleName,local_config);
									// ���������
									{
										int len=lstrlen(local_config);
										if (len>5 && !lstrcmpi(&local_config[len-5],L"\\bin\\"))
											local_config[len-4]=0;
									}
									lstrcat(local_config,L"UpdateEx.dll.local.config");
									GetPrivateProfileString(Guid,L"URLChangelog",L"",url,ARRAYSIZE(url),local_config);
								}
								if (url[0])
								{
									ShellExecute(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
									return true;
								}
								else if (Cur->Changelog[0])
								{
									ShellExecute(nullptr,L"open",Cur->Changelog,nullptr,nullptr,SW_SHOWNORMAL);
									return true;
								}
							}
							MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_F3)
						{
GOTO_F3:
							intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
							{
								wchar_t TmpFileName[MAX_PATH];
								FSF.MkTemp(TmpFileName,MAX_PATH,L"DESC");
								SECURITY_ATTRIBUTES sa;
								memset(&sa,0,sizeof(sa));
								sa.nLength=sizeof(sa);
								bool bCreate=false;
								HANDLE hFile=CreateFile(TmpFileName,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_FLAG_SEQUENTIAL_SCAN,0);
								if (hFile!=INVALID_HANDLE_VALUE)
								{
									DWORD BytesWritten;
									if (WriteFile(hFile,(LPCVOID)Cur->Description,(DWORD)(lstrlen(Cur->Description)*sizeof(wchar_t)), &BytesWritten, 0))
										bCreate=true;
									CloseHandle(hFile);
								}
								if (bCreate)
									if (Info.Viewer(TmpFileName,Cur->Title,0,0,-1,-1,VF_DISABLEHISTORY|VF_DELETEONLYFILEONCLOSE,CP_DEFAULT))
										Info.AdvControl(&MainGuid,ACTL_REDRAWALL,0,0);
								return true;
							}
							return true;
						}
						else if (vk==VK_F4)
						{
GOTO_F4:
							if (ipc.opt.Mode==MODE_NEW)
							{
								intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
								ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
								ModuleInfo *Cur=Tmp?*Tmp:nullptr;
								if (Cur)
								{
									wchar_t Buf[MAX_PATH];
									if (Info.InputBox(&MainGuid,&InputBoxGuid,MSG(MName),MSG(MEditPath),L"UpdEditPath",Cur->ModuleName,Buf,MAX_PATH,nullptr,FIB_EXPANDENV|FIB_EDITPATH|FIB_BUTTONS|FIB_ENABLEEMPTY|FIB_NOUSELASTHISTORY))
									{
										FSF.Trim(Buf);
										if (*Buf)
										{
											if (Buf[lstrlen(Buf)-1]!=L'\\') lstrcat(Buf,L"\\");
										}
										else
											GetNewModuleDir(Cur->New.ArcName,Buf);
										lstrcpy(Cur->ModuleName,Buf);
										Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgPATH,FSF.TruncPathStr(Buf,80-2-2));
										return true;
									}
								}
							}
							MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_F6)
						{
GOTO_F6:
							if (ipc.opt.Mode!=MODE_NEW && GetStatus()!=S_DOWNLOAD && GetStatus()!=S_COMPLET)
							{
								ipc.opt.Mode=MODE_UNDO;
								NeedRestart=true;
								SetStatus(S_COMPLET);
								for(size_t i=0; i<ipc.CountModules; i++)
									ipc.Modules[i].Flags&=~UPD;
								Info.SendDlgMessage(hDlg,DN_UPDDLG,0,0);
//								MakeListItemInfo(hDlg,0);
								return true;
							}
							MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_F7)
						{
GOTO_F7:
							if (GetStatus()!=S_DOWNLOAD && GetStatus()!=S_COMPLET)
							{
								if (GetNewModulesDialog())
								{
									Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
									if (WaitForSingleObject(UnlockEvent,0)==WAIT_TIMEOUT)
									{
										MessageBeep(MB_ICONASTERISK);
										Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
										return true;
									}
									ipc.opt.Mode=MODE_NEW;
									int Auto=ipc.opt.Auto;
									ipc.opt.Auto=0;
									CleanTime();
									WaitEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
									HANDLE hScreen=nullptr;
									for (;;)
									{
										hScreen=Info.SaveScreen(0,0,-1,-1);
										LPCWSTR Items[]={MSG(MName),MSG(MWait)};
										Info.Message(&MainGuid,&MsgWaitGuid, 0, nullptr, Items, ARRAYSIZE(Items), 0);
										// ������ ����� � ������� ��� ����������
										// �.�. ������ ����� � ����, ����� ����� �������� �� ����� n ���
										if (WaitForSingleObject(WaitEvent,ipc.opt.Wait*1000)!=WAIT_OBJECT_0)
										{
											const wchar_t *err;
											switch(GetStatus())
											{
												case S_CANTGETINSTALLINFO:
													err=MSG(MCantGetInstallInfo);
													break;
												case S_CANTCREATTMP:
													err=MSG(MCantCreatTmp);
													break;
												case S_CANTGETFARLIST:
													err=MSG(MCantGetFarList);
													break;
												case S_CANTGETPLUGLIST:
													err=MSG(MCantGetPlugList);
													break;
												case S_CANTGETFARUPDINFO:
													err=MSG(MCantGetFarUpdInfo);
													break;
												case S_CANTGETPLUGUPDINFO:
													err=MSG(MCantGetPlugUpdInfo);
													break;
												default:
													err=MSG(MCantConnect);
													break;
											}
											Info.RestoreScreen(hScreen);
											LPCWSTR Items[]={MSG(MName),err};
											if (Info.Message(&MainGuid,&MsgErrGuid, FMSG_MB_RETRYCANCEL|FMSG_LEFTALIGN|FMSG_WARNING, nullptr, Items, ARRAYSIZE(Items), 0))
											{
												CloseHandle(WaitEvent);
												ipc.opt.Mode=MODE_UPD;
												ipc.opt.Auto=Auto; // �����������
												if (!ipc.opt.Auto) SaveTime();
												Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
												return true;
											}
										}
										else
										{
											Info.RestoreScreen(hScreen);
											CloseHandle(WaitEvent);
											break;
										}
									}
									SaveTime();
									ipc.opt.Auto=Auto; // �����������
									MakeList(hDlg);
									Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
									MakeListItemInfo(hDlg,0);
								}
							}
							else
								MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_F8)
						{
GOTO_F8:
							if (ipc.opt.Mode==MODE_UPD && GetStatus()!=S_DOWNLOAD && GetStatus()!=S_COMPLET)
							{
								intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
								ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
								ModuleInfo *Cur=Tmp?*Tmp:nullptr;
								if (Cur && Cur->Guid!=FarGuid && Cur->Guid!=MainGuid)
								{
									LPCWSTR Items[]={MSG(MDel),MSG(MDelBody),Cur->Title};
									if (!Info.Message(&MainGuid,&MsgAskDelGuid,FMSG_MB_YESNO|FMSG_WARNING,nullptr,Items,ARRAYSIZE(Items),0))
									{
										Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,false,0);
										HANDLE hPlugin = (HANDLE)Info.PluginsControl(INVALID_HANDLE_VALUE,PCTL_FINDPLUGIN,PFM_MODULENAME,Cur->ModuleName);
										if (hPlugin && Info.PluginsControl(hPlugin,PCTL_UNLOADPLUGIN,0,nullptr))
										{
											wchar_t delpath[MAX_PATH];
											lstrcpy(delpath,Cur->ModuleName);
											*(StrRChr(delpath,nullptr,L'\\'))=0;
											// ���������
											int len=lstrlen(delpath);
											if (len>4 && !lstrcmpi(&delpath[len-4],L"\\bin"))
												delpath[len-4]=0;
											delpath[lstrlen(delpath)+1]=0;
											SHFILEOPSTRUCT fop={};
											fop.wFunc=FO_DELETE;
											fop.pFrom=delpath;
											fop.pTo = L"\0\0";
											fop.fFlags=FOF_NOCONFIRMATION|FOF_SILENT|FOF_ALLOWUNDO;
											DWORD Result=SHFileOperation(&fop);
											if (!Result && !fop.fAnyOperationsAborted)
											{
												size_t i=0;
												for( ; i<ipc.CountModules; i++)
												{
													if (Cur->ID==ipc.Modules[i].ID)
														break;
												}
												if (Cur->Flags&UPD)
													CountUpdate--;
												for(size_t j=i+1; i && i<ipc.CountModules && j<ipc.CountModules; i++,j++)
													memcpy(&ipc.Modules[i],&ipc.Modules[j],sizeof(ModuleInfo));
												ipc.Modules=(ModuleInfo *)realloc(ipc.Modules,(--ipc.CountModules)*sizeof(ModuleInfo));
												MakeList(hDlg,Pos-1);
												MakeListItemInfo(hDlg,(void*)(Pos-1));
												Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,true,0);
												return true;
											}
										}
										Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,true,0);
									}
									else
										return true;
								}
							}
							MessageBeep(MB_ICONASTERISK);
							return true;
						}
						else if (vk==VK_RETURN)
						{
							Info.SendDlgMessage(hDlg,DM_CLOSE,DlgUPD,0);
							return true;
						}
					}
				}
				if (IsShift(record))
				{
					if (Param1==DlgLIST)
					{
						intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
						ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
						ModuleInfo *Cur=Tmp?*Tmp:nullptr;
						if (Cur)
						{
							if (vk==VK_RETURN)
							{
								wchar_t url[2048], Guid[37];
								GuidToStr(Cur->Guid,Guid);
								GetPrivateProfileString(Guid,L"URLHome",L"",url,ARRAYSIZE(url),ipc.Config);
								if (url[0]==0)
								{
									wchar_t local_config[MAX_PATH];
									GetModuleDir(Cur->ModuleName,local_config);
									// ���������
									{
										int len=lstrlen(local_config);
										if (len>5 && !lstrcmpi(&local_config[len-5],L"\\bin\\"))
											local_config[len-4]=0;
									}
									lstrcat(local_config,L"UpdateEx.dll.local.config");
									GetPrivateProfileString(Guid,L"URLHome",L"",url,ARRAYSIZE(url),local_config);
								}
								if (url[0])
								{
									ShellExecute(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
									return true;
								}
								else if (Cur->Guid==FarGuid || (Cur->Flags&STD))
								{
									ShellExecute(nullptr,L"open",L"http://www.farmanager.com/nightly.php",nullptr,nullptr,SW_SHOWNORMAL);
									return true;
								}
								else if (Cur->pid[0])
								{
									lstrcpy(url,L"http://plugring.farmanager.com/plugin.php?pid=");
									lstrcat(url,Cur->pid);
									ShellExecute(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
									return true;
								}
								else
									MessageBeep(MB_OK);
							}
							else if (vk==VK_INSERT)
							{
								Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
								if (ShowSendModulesDialog(Cur))
									Info.SendDlgMessage(hDlg,DM_CLOSE,DlgCANCEL,0);
								else
									Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
								return true;
							}
						}
					}
				}
				if (IsCtrl(record))
				{
					if (Param1==DlgLIST)
					{
						if (vk==0x48) // VK_H
						{
GOTO_CTRLH:
							if (GetStatus()!=S_DOWNLOAD && ipc.opt.Mode==MODE_UPD)
							{
								ipc.opt.ShowDisable=ipc.opt.ShowDisable?0:1;
								PluginSettings settings(MainGuid, Info.SettingsControl);
								settings.Set(0,L"ShowDisable",ipc.opt.ShowDisable);
								MakeList(hDlg);
								MakeListItemInfo(hDlg,0);
							}
							else
								MessageBeep(MB_OK);
							return true;
						}
						else if (vk==0x30) // VK_0
						{
							if (GetStatus()!=S_DOWNLOAD)
							{
								ipc.opt.ShowDraw=ipc.opt.ShowDraw?0:1;
								PluginSettings settings(MainGuid, Info.SettingsControl);
								settings.Set(0,L"ShowDraw",ipc.opt.ShowDraw);
								MakeList(hDlg,-1);
							}
							else
								MessageBeep(MB_OK);
							return true;
						}
						else if (vk==0x31) // VK_1
						{
							if (GetStatus()!=S_DOWNLOAD && ipc.opt.Mode!=MODE_NEW)
							{
								ipc.opt.ShowDate=ipc.opt.ShowDate?0:1;
								PluginSettings settings(MainGuid, Info.SettingsControl);
								settings.Set(0,L"ShowDate",ipc.opt.ShowDate);
								MakeList(hDlg,-1);
							}
							else
								MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_INSERT)
						{
							intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
								FSF.CopyToClipboard(FCT_STREAM,Cur->ModuleName);
							return true;
						}
					}
				}
				if (Param1==DlgLIST)
				{
//					struct WindowType Type={sizeof(WindowType)};
//					if (Info.AdvControl(&MainGuid,ACTL_GETWINDOWTYPE,0,&Type) && (Type.Type==WTYPE_PANELS))
					{
						bool isLActive=false;
						struct PanelInfo PInfo={sizeof(PanelInfo)};
						if (Info.PanelControl(PANEL_ACTIVE,FCTL_GETPANELINFO,0,&PInfo))
							isLActive=(PInfo.PanelType==PTYPE_FILEPANEL && PInfo.Flags&PFLAGS_PANELLEFT);
						bool isCtrl=IsCtrl(record);
						bool isCtrlShift=(record->Event.KeyEvent.dwControlKeyState&(RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED|SHIFT_PRESSED))!=0;

						if ((isCtrl && vk==VK_PRIOR) || (isCtrlShift && vk==VK_PRIOR)) //PgUp
						{
							if (ipc.opt.Mode!=MODE_NEW)
							{
								intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
								ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
								ModuleInfo *Cur=Tmp?*Tmp:nullptr;
								if (Cur)
								{
									Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
									wchar_t PluginDirectory[MAX_PATH];
									GetModuleDir(Cur->ModuleName,PluginDirectory);
									FarPanelDirectory dirInfo={sizeof(FarPanelDirectory),PluginDirectory,nullptr,{0},nullptr};
									Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETPANELDIRECTORY,0,&dirInfo);
									Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETACTIVEPANEL,0,0);
									Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
									return true;
								}
							}
							Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
							MessageBeep(MB_OK);
							return true;
						}
						else if ((isCtrl && vk==VK_NEXT) || (isCtrlShift && vk==VK_NEXT))//PgDn
						{
							intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
							{
								if (*Cur->New.ArcName)
								{
									wchar_t arc[MAX_PATH];
									lstrcpy(arc,ipc.TempDirectory);
									lstrcat(arc,Cur->New.ArcName);
									if(GetFileAttributes(arc)!=INVALID_FILE_ATTRIBUTES)
									{
										Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
										FarPanelDirectory dirInfo={sizeof(FarPanelDirectory),L"\\",nullptr,ArcliteGuid,arc};
										Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETPANELDIRECTORY,0,&dirInfo);
										Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETACTIVEPANEL,0,0);
										Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
										return true;
									}
								}
							}
							Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
							MessageBeep(MB_OK);
							return true;
						}
						else if ((isCtrl && vk==VK_HOME) || (isCtrlShift && vk==VK_HOME))
						{
							Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
							FarPanelDirectory dirInfo={sizeof(FarPanelDirectory),ipc.TempDirectory,nullptr,{0},nullptr};
							Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETPANELDIRECTORY,0,&dirInfo);
							Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETACTIVEPANEL,0,0);
							Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
							return true;
						}
					}
				}
			}
			break;
		}

		/************************************************************************/

		case DN_UPDDLG:
		{
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,false,0);
			if (ipc.opt.Mode==MODE_UNDO)
				Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgBORDER,(void*)MSG(MUndoUpdates));
			if (GetStatus()==S_COMPLET)
				Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgUPD,(void*)MSG(ipc.opt.Mode==MODE_NEW?MInstall:(ipc.opt.Mode==MODE_UNDO?MUndo:MUpdate)));
			Info.SendDlgMessage(hDlg,DM_ENABLE,DlgUPD,(void*)1);
			MakeList(hDlg,-1);
			if (GetStatus()==S_COMPLET)
			{
				intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
				MakeListItemInfo(hDlg,(void*)Pos);
			}
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,true,0);
			break;
		}
		/************************************************************************/

		case DN_CLOSE:
		{
			bool ret=false;
			if (Param1==DlgUPD)
			{
				switch(GetStatus())
				{
					case S_UPDATE:
					case S_UPTODATE:
					{
						SetStatus(S_DOWNLOAD);
						Info.SendDlgMessage(hDlg,DM_ENABLE,DlgUPD,0);
						return false;
					}
					case S_COMPLET:
					{
						for(size_t i=0; i<ipc.CountModules; i++)
							if (ipc.Modules[i].Flags&UPD)
							{
								ret=true;
								break;
							}
						break;
					}
					default:
						return false;
				}
			}
			else if (Param1==DlgCANCEL || Param1==-1)
			{
				if (GetStatus()==S_DOWNLOAD)
				{
					LPCWSTR Items[]={MSG(MName),MSG(MAskCancelDownload)};
					if (Info.Message(&MainGuid,&MsgAskCancelDownloadGuid,FMSG_MB_YESNO|FMSG_WARNING,nullptr,Items,ARRAYSIZE(Items),0))
						return false;
				}
				SetStatus(S_NONE);
				ret=true;
			}
			if (ret)
			{
				wchar_t guid[38]; //36 ���� + 1 �� �����������
				wchar_t *buf=(wchar_t*)malloc(ipc.CountModules*ARRAYSIZE(guid)*sizeof(wchar_t));
				if (buf) buf[0]=0;
				bool Set=false;
				for(size_t i=0; i<ipc.CountModules; i++)
				{
					if (buf && ((ipc.Modules[i].Flags&SKIP) || (ipc.opt.Mode==MODE_UNDO && (ipc.Modules[i].Flags&UPD))))
					{
						Set=true;
						lstrcat(buf,GuidToStr(ipc.Modules[i].Guid,guid));
						lstrcat(buf,L",");
					}
				}
				PluginSettings settings(MainGuid, Info.SettingsControl);
				if (Set)
					settings.Set(0,L"Skip",buf);
				else
					settings.DeleteValue(0,L"Skip");
				if(buf) free(buf);
			}
			return ret;
		}
	}
	return Info.DefDlgProc(hDlg,Msg,Param1,Param2);
}

bool ShowModulesDialog()
{
	struct FarDialogItem DialogItems[] = {
		//			Type	X1	Y1	X2	Y2	Selected	History	Mask	Flags	Data	MaxLen	UserParam
		/* 0*/{DI_DOUBLEBOX,  0, 0,80,25, 0, 0, 0,                             0, MSG(MAvailableUpdates),0,0},
		/* 1*/{DI_LISTBOX,    1, 1,78,15, 0, 0, 0, DIF_FOCUS|DIF_LISTNOCLOSE|DIF_LISTNOBOX,0,0,0},
		/* 2*/{DI_TEXT,      -1,16, 0, 0, 0, 0, 0,            DIF_SEPARATOR,MSG(MListButton),0,0},
		/* 3*/{DI_TEXT,       2,17,78,17,78, 0, 0,                    DIF_SHOWAMPERSAND, L"",0,0},
		/* 4*/{DI_TEXT,       2,18,78,18,78, 0, 0,                    DIF_SHOWAMPERSAND, L"",0,0},
		/* 5*/{DI_TEXT,       2,19,78,19,78, 0, 0,                    DIF_SHOWAMPERSAND, L"",0,0},
		/* 6*/{DI_TEXT,       2,20,78,20,78, 0, 0,                    DIF_SHOWAMPERSAND, L"",0,0},
		/* 7*/{DI_TEXT,       2,21,78,21,78, 0, 0,                    DIF_SHOWAMPERSAND, L"",0,0},
		/* 8*/{DI_TEXT,      -1,22, 0, 0, 0, 0, 0,                       DIF_SEPARATOR2, L"",0,0},
		/* 9*/{DI_BUTTON,     0,23, 0, 0, 0, 0, 0, DIF_DEFAULTBUTTON|DIF_CENTERGROUP, MSG(MDownload),0,0},
		/*10*/{DI_BUTTON,     0,23, 0, 0, 0, 0, 0,             DIF_CENTERGROUP, MSG(MCancel),0,0}
	};

	bool ret=false;
	hDlg=Info.DialogInit(&MainGuid, &ModulesDlgGuid,-1,-1,80,25,L"Contents",DialogItems,ARRAYSIZE(DialogItems),0,FDLG_SMALLDIALOG,ShowModulesDialogProc,0);

	if (hDlg != INVALID_HANDLE_VALUE)
	{
		if (Info.DialogRun(hDlg)==DlgUPD)
			ret=true;
		Info.DialogFree(hDlg);
	}
	hDlg=nullptr;
	return ret;
}

VOID ReadSettings()
{
	PluginSettings settings(MainGuid, Info.SettingsControl);
	ipc.opt.Stable=settings.Get(0,L"Stable",0);
	ipc.opt.Auto=settings.Get(0,L"Auto",0);
	ipc.opt.TrayNotify=settings.Get(0,L"TrayNotify",0);
	ipc.opt.Wait=settings.Get(0,L"Wait",10);
	ipc.opt.SaveFarAfterInstall=settings.Get(0,L"SaveFar",2);
	ipc.opt.SavePlugAfterInstall=settings.Get(0,L"SavePlug",2);
	ipc.opt.Date=settings.Get(0,L"Date",0);
	ipc.opt.Autoload=settings.Get(0,L"Autoload",0);
	ipc.opt.Proxy=settings.Get(0,L"Proxy",0);
	settings.Get(0,L"Srv",ipc.opt.ProxyName,ARRAYSIZE(ipc.opt.ProxyName),L"");
	settings.Get(0,L"User",ipc.opt.ProxyUser,ARRAYSIZE(ipc.opt.ProxyUser),L"");
	settings.Get(0,L"Pass",ipc.opt.ProxyPass,ARRAYSIZE(ipc.opt.ProxyPass),L"");
	ipc.opt.ShowDisable=settings.Get(0,L"ShowDisable",1);
	ipc.opt.Mode=MODE_UPD;
	ipc.opt.DateFrom[0]=ipc.opt.DateTo[0]=0;
	ipc.opt.ShowDate=settings.Get(0,L"ShowDate",0);
	ipc.opt.ShowDraw=settings.Get(0,L"ShowDraw",1);

	wchar_t Buf[MAX_PATH];
	settings.Get(0,L"Dir",Buf,ARRAYSIZE(Buf),L"%TEMP%\\UpdateEx\\");
	lstrcpy(ipc.opt.TempDirectory,Buf);
	ExpandEnvironmentStrings(Buf,ipc.TempDirectory,ARRAYSIZE(ipc.TempDirectory));
	if (ipc.TempDirectory[lstrlen(ipc.TempDirectory)-1]!=L'\\') lstrcat(ipc.TempDirectory,L"\\");
	lstrcat(lstrcpy(ipc.FarUpdateList,ipc.TempDirectory),FarUpdateFile);
	lstrcat(lstrcpy(ipc.PlugUpdateList,ipc.TempDirectory),PlugUpdateFile);
	lstrcat(lstrcpy(ipc.Config,Info.ModuleName),L".config");
	lstrcat(lstrcpy(PluginModule,ipc.TempDirectory),StrRChr(Info.ModuleName,nullptr,L'\\')+1);
	lstrcat(lstrcpy(ipc.Cache,PluginModule),
#ifdef _WIN64
                   L".x64.cache"
#else
                   L".x86.cache"
#endif
	);
}

intptr_t Config()
{
	wchar_t num[64];

	struct FarDialogItem DialogItems[] = {
		//			Type	X1	Y1	X2	Y2				Selected	History	Mask	Flags	Data	MaxLen	UserParam
		/* 0*/{DI_DOUBLEBOX,  3, 1,60,17, 0, 0, 0,                  0,MSG(MName),0,0},
		/* 1*/{DI_CHECKBOX,   5, 2, 0, 0, ipc.opt.Stable, 0, 0, DIF_FOCUS, MSG(MCfgStable),0,0},
		/* 2*/{DI_CHECKBOX,   5, 3, 0, 0, ipc.opt.Auto, 0, 0,           0, MSG(MCfgAuto),0,0},
		/* 3*/{DI_CHECKBOX,   9, 4, 0, 0, ipc.opt.TrayNotify, 0, 0,     0, MSG(MCfgTrayNotify),0,0},
		/* 4*/{DI_FIXEDIT,    5, 5, 7, 0, 0, 0, L"999",  DIF_MASKEDIT,FSF.itoa(ipc.opt.Wait,num,10),0,0},
		/* 5*/{DI_TEXT,       9, 5,58, 0, 0, 0, 0,                  0,MSG(MCfgWait),0,0},
		/* 6*/{DI_CHECKBOX,   5, 6, 0, 0, ipc.opt.SaveFarAfterInstall,0, 0,DIF_3STATE,MSG(MCfgSaveFar),0,0},
		/* 7*/{DI_CHECKBOX,   5, 7, 0, 0, ipc.opt.SavePlugAfterInstall,0, 0,DIF_3STATE,MSG(MCfgSavePlug),0,0},
		/* 8*/{DI_CHECKBOX,   5, 8, 0, 0, ipc.opt.Date,  0, 0,          0,MSG(MCfgDate),0,0},
		/* 9*/{DI_CHECKBOX,   5, 9, 0, 0, ipc.opt.Autoload,  0, 0,      0,MSG(MCfgAutoload),0,0},
		/*10*/{DI_CHECKBOX,   5,10, 0, 0, ipc.opt.Proxy, 0, 0,          0,MSG(MCfgProxy),0,0},
		/*11*/{DI_TEXT,       9,11,22, 0, 0,         0, 0,          0,MSG(MCfgProxySrv),0,0},
		/*12*/{DI_EDIT,      24,11,58, 0, 0,L"UpdCfgSrv",0, DIF_HISTORY,ipc.opt.ProxyName,0,0},
		/*13*/{DI_TEXT,       9,12,22, 0, 0,         0, 0,          0,MSG(MCfgPtoxyUserPass),0,0},
		/*14*/{DI_EDIT,      24,12,41, 0, 0,L"UpdCfgUser",0,DIF_HISTORY,ipc.opt.ProxyUser,0,0},
		/*15*/{DI_PSWEDIT,   43,12,58, 0, 0,         0, 0,          0,ipc.opt.ProxyPass,0,0},
		/*16*/{DI_TEXT,       5,13,58, 0, 0,         0, 0,          0,MSG(MCfgDir),0,0},
		/*17*/{DI_EDIT,       5,14,58, 0, 0,L"UpdCfgDir",0, DIF_HISTORY,ipc.opt.TempDirectory,0,0},
		/*18*/{DI_TEXT,      -1,15, 0, 0, 0,         0, 0, DIF_SEPARATOR, L"",0,0},
		/*19*/{DI_BUTTON,     0,16, 0, 0, 0,         0, 0, DIF_DEFAULTBUTTON|DIF_CENTERGROUP, MSG(MOK),0,0},
		/*20*/{DI_BUTTON,     0,16, 0, 0, 0,         0, 0, DIF_CENTERGROUP, MSG(MCancel),0,0}
	};

	HANDLE hDlg=Info.DialogInit(&MainGuid, &CfgDlgGuid,-1,-1,64,19,L"Config",DialogItems,ARRAYSIZE(DialogItems),0,0,0,0);

	intptr_t ret=0;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		if (Info.DialogRun(hDlg)==19)
		{
			ipc.opt.Stable=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,1,0);
			ipc.opt.Auto=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,2,0);
			if (ipc.opt.Auto)
				ipc.opt.TrayNotify=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,3,0);
			else
				ipc.opt.TrayNotify=0;
			ipc.opt.Wait=FSF.atoi((const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,4,0));
			if (!ipc.opt.Wait) ipc.opt.Wait=10;
			ipc.opt.SaveFarAfterInstall=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,6,0);
			ipc.opt.SavePlugAfterInstall=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,7,0);
			ipc.opt.Date=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,8,0);
			ipc.opt.Autoload=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,9,0);
			ipc.opt.Proxy=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,10,0);
			if (ipc.opt.Proxy)
			{
				lstrcpy(ipc.opt.ProxyName,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,12,0));
				lstrcpy(ipc.opt.ProxyUser,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,14,0));
				lstrcpy(ipc.opt.ProxyPass,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,15,0));
			}
			wchar_t Buf[MAX_PATH];
			lstrcpy(Buf,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,17,0));
			if (Buf[0]==0) lstrcpy(Buf,L"%TEMP%\\UpdateEx\\");
			lstrcpy(ipc.opt.TempDirectory,Buf);
			ExpandEnvironmentStrings(Buf,ipc.TempDirectory,ARRAYSIZE(ipc.TempDirectory));
			if (ipc.TempDirectory[lstrlen(ipc.TempDirectory)-1]!=L'\\') lstrcat(ipc.TempDirectory,L"\\");
			lstrcat(lstrcpy(ipc.FarUpdateList,ipc.TempDirectory),FarUpdateFile);
			lstrcat(lstrcpy(ipc.PlugUpdateList,ipc.TempDirectory),PlugUpdateFile);

			PluginSettings settings(MainGuid, Info.SettingsControl);
			settings.Set(0,L"Stable",ipc.opt.Stable);
			settings.Set(0,L"Auto",ipc.opt.Auto);
			settings.Set(0,L"TrayNotify",ipc.opt.TrayNotify);
			settings.Set(0,L"Wait",ipc.opt.Wait);
			settings.Set(0,L"SaveFar",ipc.opt.SaveFarAfterInstall);
			settings.Set(0,L"SavePlug",ipc.opt.SavePlugAfterInstall);
			settings.Set(0,L"Date",ipc.opt.Date);
			settings.Set(0,L"Autoload",ipc.opt.Autoload);
			settings.Set(0,L"Proxy",ipc.opt.Proxy);
			settings.Set(0,L"Srv",ipc.opt.ProxyName);
			settings.Set(0,L"User",ipc.opt.ProxyUser);
			settings.Set(0,L"Pass",ipc.opt.ProxyPass);
			settings.Set(0,L"Dir",ipc.opt.TempDirectory);
			ret=1;
		}
		Info.DialogFree(hDlg);
	}
	return ret;
}

#include "AutoloaderThread.cpp"
#include "NotifyThread.cpp"

DWORD WINAPI ThreadProc(LPVOID /*lpParameter*/)
{
	while(WaitForSingleObject(StopEvent, 0)!=WAIT_OBJECT_0)
	{
		ExitFAR=false;
		bool Time=false;
		Time=IsTime();
		if (Time)
		{
			ResetEvent(UnlockEvent); // ������ �� ���������� ������ �� F11
			if (ipc.opt.Auto)
			{
				HANDLE hEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
				EventStruct es={E_LOADPLUGINS,hEvent};
				Info.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, &es);
				WaitForSingleObject(hEvent,INFINITE);
				CloseHandle(hEvent);
				GetUpdModulesInfo();
			}
			else
			{
				if (WaitForSingleObject(WaitEvent,0)==WAIT_TIMEOUT)
					GetUpdModulesInfo();
			}
			if (GetStatus()==S_UPDATE || GetStatus()==S_UPTODATE)
			{
				if (WaitEvent) SetEvent(WaitEvent);
				if (GetStatus()==S_UPDATE && ipc.opt.Auto)
				{
					SetStatus(S_DOWNLOAD);
					if (ipc.opt.TrayNotify)
						hNotifyThread=CreateThread(nullptr,0,&NotifyProc,nullptr,0,0);
					DownloadUpdates();
					if (GetStatus()==S_COMPLET)
					{
						for (;;)
						{
							struct WindowType Type={sizeof(WindowType)};
							if (Info.AdvControl(&MainGuid,ACTL_GETWINDOWTYPE,0,&Type) && Type.Type==WTYPE_PANELS)
								break;
							Sleep(100);
						}
						bool Cancel=true;
						HANDLE hEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
						EventStruct es={E_ASKUPD,hEvent,&Cancel};
						Info.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, &es);
						WaitForSingleObject(hEvent,INFINITE);
						CloseHandle(hEvent);
						if (!Cancel)
							StartUpdate(true);
					}
				}
			}
			SaveTime();
			SetEvent(UnlockEvent);
		}
		else if (GetStatus()==S_DOWNLOAD)
		{
			ResetEvent(UnlockEvent); // ������ �� ���������� ������ �� F11
			DownloadUpdates();
			SetEvent(UnlockEvent);
		}
		if (ExitFAR) Info.AdvControl(&MainGuid,ACTL_QUIT,0,0);
		Sleep(100);
	}
	return 0;
}

INT WINAPI GetMinFarVersionW()
{
#define MAKEFARVERSION(major,minor,build) ( ((major)<<8) | (minor) | ((build)<<16))
	return MAKEFARVERSION(MIN_FAR_MAJOR_VER,MIN_FAR_MINOR_VER,MIN_FAR_BUILD);
#undef MAKEFARVERSION
}

void WINAPI GetGlobalInfoW(GlobalInfo *Info)
{
	Info->StructSize=sizeof(GlobalInfo);
	Info->MinFarVersion=MAKEFARVERSION(MIN_FAR_MAJOR_VER,MIN_FAR_MINOR_VER,0,MIN_FAR_BUILD,VS_RELEASE);
	Info->Version=MAKEFARVERSION(MAJOR_VER,MINOR_VER,0,BUILD,VS_RELEASE);
	Info->Guid=MainGuid;
	Info->Title=L"UpdateEx";
	Info->Description=L"Update plugin for Far Manager v.3.0";
	Info->Author=L"Alex Alabuzhev, Alexey Samlyukov";
}

VOID WINAPI SetStartupInfoW(const PluginStartupInfo* psInfo)
{
	ifn.Load();
	Info=*psInfo;
	FSF=*psInfo->FSF;
	Info.FSF=&FSF;
	ipc.Modules=nullptr;
	ipc.CountModules=0;
	ReadSettings();
	InitializeCriticalSection(&cs);
	StopEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);
	UnlockEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);
	hThread=CreateThread(nullptr,0,ThreadProc,nullptr,0,nullptr);
	hExitAutoloadThreadEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);
	hAutoloadThread_1=CreateThread(nullptr,0,AutoloadThreadProc,nullptr,0,nullptr);
	bool bAltPluginsDir=true;
	hAutoloadThread_2=CreateThread(nullptr,0,AutoloadThreadProc,&bAltPluginsDir,0,nullptr);
}

VOID WINAPI GetPluginInfoW(PluginInfo* pInfo)
{
	pInfo->StructSize=sizeof(PluginInfo);
	static LPCWSTR PluginMenuStrings[1],PluginConfigStrings[1];
	PluginMenuStrings[0]=MSG(MName);
	pInfo->PluginMenu.Guids = &MenuGuid;
	pInfo->PluginMenu.Strings = PluginMenuStrings;
	pInfo->PluginMenu.Count=ARRAYSIZE(PluginMenuStrings);
	PluginConfigStrings[0]=MSG(MName);
	pInfo->PluginConfig.Guids = &CfgMenuGuid;
	pInfo->PluginConfig.Strings = PluginConfigStrings;
	pInfo->PluginConfig.Count = ARRAYSIZE(PluginConfigStrings);
	pInfo->Flags=PF_PRELOAD;
	static LPCWSTR CommandPrefix=L"updex";
	pInfo->CommandPrefix=CommandPrefix;
}

intptr_t WINAPI ConfigureW(const ConfigureInfo* Info)
{
	return Config();
}

VOID WINAPI ExitFARW(ExitInfo* Info)
{
	if (hExitAutoloadThreadEvent)
		SetEvent(hExitAutoloadThreadEvent);

	if (hNotifyThread)
	{
		DWORD ec = 0;
		if (GetExitCodeThread(hNotifyThread, &ec) == STILL_ACTIVE)
			TerminateThread(hNotifyThread, ec);
		WaitForSingleObject(hNotifyThread,INFINITE);
		CloseHandle(hNotifyThread);
		hNotifyThread=nullptr;
	}

	if (hAutoloadThread_1)
	{
		WaitForSingleObject(hAutoloadThread_1,INFINITE);
		CloseHandle(hAutoloadThread_1);
		hAutoloadThread_1=nullptr;
	}
	if (hAutoloadThread_2)
	{
		WaitForSingleObject(hAutoloadThread_2,INFINITE);
		CloseHandle(hAutoloadThread_2);
		hAutoloadThread_2=nullptr;
	}
	CloseHandle(hExitAutoloadThreadEvent);

	if (hThread)
	{
		SetEvent(StopEvent);
		if (WaitForSingleObject(hThread,300)== WAIT_TIMEOUT)
		{
			DWORD ec = 0;
			if (GetExitCodeThread(hThread, &ec) == STILL_ACTIVE)
				TerminateThread(hThread, ec);
			WaitForSingleObject(hThread,INFINITE);
		}
		CloseHandle(hThread);
		hThread=nullptr;
	}
	DeleteCriticalSection(&cs);
	CloseHandle(StopEvent);
	CloseHandle(UnlockEvent);
	if (hRunDll) CloseHandle(hRunDll);
	FreeModulesInfo();
	Clean();
}

HANDLE WINAPI OpenW(const OpenInfo* oInfo)
{
	if (WaitForSingleObject(UnlockEvent,0)==WAIT_TIMEOUT)
	{
		MessageBeep(MB_ICONASTERISK);
		return nullptr;
	}
	ipc.opt.Mode=MODE_UPD;
	ipc.opt.DateFrom[0]=ipc.opt.DateTo[0]=0;
	ExitFAR=false;
	int Auto=ipc.opt.Auto;
	ipc.opt.Auto=0;
	CleanTime();
	WaitEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
	HANDLE hScreen=nullptr;
	for (;;)
	{
		hScreen=Info.SaveScreen(0,0,-1,-1);
		LPCWSTR Items[]={MSG(MName),MSG(MWait)};
		Info.Message(&MainGuid,&MsgWaitGuid, 0, nullptr, Items, ARRAYSIZE(Items), 0);
		// ������ ����� � ������� ��� ����������
		// �.�. ������ ����� � ����, ����� ����� �������� �� ����� n ���
		if (WaitForSingleObject(WaitEvent,ipc.opt.Wait*1000)!=WAIT_OBJECT_0)
		{
			const wchar_t *err;
			switch(GetStatus())
			{
				case S_CANTGETINSTALLINFO:
					err=MSG(MCantGetInstallInfo);
					break;
				case S_CANTCREATTMP:
					err=MSG(MCantCreatTmp);
					break;
				case S_CANTGETFARLIST:
					err=MSG(MCantGetFarList);
					break;
				case S_CANTGETPLUGLIST:
					err=MSG(MCantGetPlugList);
					break;
				case S_CANTGETFARUPDINFO:
					err=MSG(MCantGetFarUpdInfo);
					break;
				case S_CANTGETPLUGUPDINFO:
					err=MSG(MCantGetPlugUpdInfo);
					break;
				default:
					err=MSG(MCantConnect);
					break;
			}
			Info.RestoreScreen(hScreen);
			LPCWSTR Items[]={MSG(MName),err};
			if (Info.Message(&MainGuid,&MsgErrGuid, FMSG_MB_RETRYCANCEL|FMSG_LEFTALIGN|FMSG_WARNING, nullptr, Items, ARRAYSIZE(Items), 0))
			{
				CloseHandle(WaitEvent);
				ipc.opt.Auto=Auto; // �����������
				if (!ipc.opt.Auto) SaveTime();
				return nullptr;
			}
		}
		else
		{
			Info.RestoreScreen(hScreen);
			CloseHandle(WaitEvent);
			break;
		}
	}

	if (ShowModulesDialog())
		StartUpdate(false);

	SaveTime();
	ipc.opt.Auto=Auto; // �����������
	Clean();
	if (ExitFAR) Info.AdvControl(&MainGuid,ACTL_QUIT,0,0);

	return nullptr;
}

intptr_t WINAPI ProcessSynchroEventW(const ProcessSynchroEventInfo *pInfo)
{
	switch(pInfo->Event)
	{
		case SE_COMMONSYNCHRO:
		{
			EventStruct* es=reinterpret_cast<EventStruct*>(pInfo->Param);
			switch(es->Event)
			{
				case E_LOADPLUGINS:
				{
					SetEvent(reinterpret_cast<HANDLE>(es->Data));
					break;
				}
				case E_ASKUPD:
				{
					if (ShowModulesDialog())
						*es->Result=false;
					SetEvent(reinterpret_cast<HANDLE>(es->Data));
					break;
				}
				case E_EXIT:
				{
					LPCWSTR Items[]={MSG(MName),MSG(MExitFAR),MSG(MExitFARAsk)};
					if ((ExitFAR=!Info.Message(&MainGuid,&MsgExitFARGuid, FMSG_MB_YESNO, nullptr, Items, ARRAYSIZE(Items), 0))==0)
					{
						Console console;
						TextColor color(FOREGROUND_GREEN|FOREGROUND_INTENSITY);
						mprintf(MSG(MExitFAR));
						mprintf(L"\n\n");
					}
					SetEvent(reinterpret_cast<HANDLE>(es->Data));
					break;
				}
				case E_CANTCOMPLETE:
				{
					LPCWSTR Items[]={MSG(MName),MSG(MCantCompleteUpd),MSG(MExitFAR)};
					Info.Message(&MainGuid,&MsgCantCompleteUpdGuid, FMSG_WARNING|FMSG_MB_OK, nullptr, Items, ARRAYSIZE(Items), 0);
					SetEvent(reinterpret_cast<HANDLE>(es->Data));
					break;
				}
			}
		}
		break;
	}
	return 0;
}

#include "InstallCorrection.cpp"

EXTERN_C VOID WINAPI RestartFARW(HWND,HINSTANCE,LPCWSTR lpCmd,DWORD)
{
	ifn.Load();
	INT argc=0;
	LPWSTR *argv=CommandLineToArgvW(lpCmd,&argc);
	if(argc==2)
	{
		if(!ifn.AttachConsole(ATTACH_PARENT_PROCESS))
		{
			AllocConsole();
		}
		INT_PTR ptr=StringToNumber(argv[0],ptr);
		HANDLE hFar=static_cast<HANDLE>(reinterpret_cast<LPVOID>(ptr));
		if(hFar && hFar!=INVALID_HANDLE_VALUE)
		{
			HMENU hMenu=GetSystemMenu(GetConsoleWindow(),FALSE);
			INT Count=GetMenuItemCount(hMenu);
			INT Pos=-1;
			for(int i=0;i<Count;i++)
			{
				if(GetMenuItemID(hMenu,i)==SC_CLOSE)
				{
					Pos=i;
					break;
				}
			}
			MENUITEMINFO mi={sizeof(mi),MIIM_ID,0,0,0};
			if(Pos!=-1)
			{
				SetMenuItemInfo(hMenu,Pos,MF_BYPOSITION,&mi);
			}
			INT_PTR IPCPtr=StringToNumber(argv[1],IPCPtr);
			if(ReadProcessMemory(hFar,reinterpret_cast<LPCVOID>(IPCPtr),&ipc,sizeof(IPC),nullptr))
			{
				ModuleInfo *MInfo=(ModuleInfo*)malloc(ipc.CountModules*sizeof(ModuleInfo));
				if (MInfo && ReadProcessMemory(hFar,reinterpret_cast<LPCVOID>(ipc.Modules),MInfo,ipc.CountModules*sizeof(ModuleInfo),nullptr))
				{
					WaitForSingleObject(hFar,INFINITE);

					CONSOLE_SCREEN_BUFFER_INFO csbi;
					GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&csbi);
					while(csbi.dwSize.Y--)
						mprintf(L"\n");
					CloseHandle(hFar);
					mprintf(L"\n\n\n");

					HMODULE hSevenZip=nullptr;
					wchar_t SevenZip[MAX_PATH]=L"";
					wchar_t TmpSevenZip[MAX_PATH]=L"";
					bool bDelSevenZip=false;

					for (size_t i=0; i<ipc.CountModules; i++)
					{
						if (MInfo[i].Guid==MainGuid)
						{
							GetModuleDir(MInfo[i].ModuleName,SevenZip);
							lstrcat(SevenZip,L"7z.dll");
						}
						if (MInfo[i].Guid==ArcliteGuid)
						{
							GetModuleDir(MInfo[i].ModuleName,TmpSevenZip);
							lstrcat(TmpSevenZip,L"7z.dll");
						}
					}
					if (!(hSevenZip=LoadLibrary(SevenZip)))
					{
						if (GetFileAttributes(TmpSevenZip)==INVALID_FILE_ATTRIBUTES)
						{
							if (!(hSevenZip=LoadLibrary(lstrcat(get_7z_path(HKEY_CURRENT_USER,SevenZip),L"7z.dll"))))
								hSevenZip=LoadLibrary(lstrcat(get_7z_path(HKEY_LOCAL_MACHINE,SevenZip),L"7z.dll"));
						}
						else
						{
							lstrcat(lstrcpy(SevenZip,ipc.TempDirectory),L"7z.dll");
							if (CopyFile(TmpSevenZip,SevenZip,FALSE))
							{
								bDelSevenZip=true;
								hSevenZip=LoadLibrary(SevenZip);
							}
						}
					}

					if (!hSevenZip)
					{
						TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
						mprintf(L"Can't load 7z.dll\n");
					}
					else
					{
						wchar_t CurDirectory[MAX_PATH];
						GetModuleDir(MInfo[0].ModuleName,CurDirectory);
						bool bPreInstall=true;
						Exec(bPreInstall,nullptr,ipc.Config,CurDirectory);

						for (size_t i=0; i<ipc.CountModules; i++)
						{
							if (MInfo[i].Flags&UPD)
							{
								wchar_t destpath[MAX_PATH];
								if (MInfo[i].Flags&NEW) lstrcpy(destpath,MInfo[i].ModuleName);
								else GetModuleDir(MInfo[i].ModuleName,destpath);
								// ���������
								{
									int len=lstrlen(destpath);
									if (len>5 && !lstrcmpi(&destpath[len-5],L"\\bin\\"))
										destpath[len-4]=0;
								}
								const wchar_t *arc=ipc.opt.Mode==MODE_UNDO?MInfo[i].Undo.ArcName:MInfo[i].New.ArcName;
								if (*arc)
								{
									wchar_t local_arc[MAX_PATH];
									lstrcpy(local_arc,ipc.TempDirectory);
									lstrcat(local_arc,arc);

									if(GetFileAttributes(local_arc)!=INVALID_FILE_ATTRIBUTES)
									{
										bPreInstall=true;
										// ������� ���������
										wchar_t local_config[MAX_PATH];
										lstrcpy(local_config,destpath);
										lstrcat(local_config,L"UpdateEx.dll.local.config");
										Exec(bPreInstall,&MInfo[i].Guid,local_config,destpath);
										// � ������ ����������
										Exec(bPreInstall,&MInfo[i].Guid,ipc.Config,destpath);

										bool Result=false;
										wchar_t BakName[MAX_PATH];
										if (!(MInfo[i].Flags&NEW) && MInfo[i].Guid!=FarGuid)
										{
											lstrcat(lstrcpy(BakName,ipc.TempDirectory),StrRChr(MInfo[i].ModuleName,nullptr,L'\\')+1);
											CopyFile(MInfo[i].ModuleName,BakName,FALSE);
											DeleteFile(MInfo[i].ModuleName);
										}
										while(!Result)
										{
											TextColor color(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
											mprintf(L"\nUnpacking %-50s",arc);

											if(!extract(hSevenZip,local_arc,destpath))
											{
												TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
												mprintf(L"\nUnpack error. Retry? (Y/N) ");
												INPUT_RECORD ir={0};
												DWORD n;
												while(!(ir.EventType==KEY_EVENT && !ir.Event.KeyEvent.bKeyDown && (ir.Event.KeyEvent.wVirtualKeyCode==L'Y'||ir.Event.KeyEvent.wVirtualKeyCode==L'N')))
												{
													ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE),&ir,1,&n);
													Sleep(1);
												}
												if(ir.Event.KeyEvent.wVirtualKeyCode==L'N')
												{
													mprintf(L"\n");
													break;
												}
												mprintf(L"\n");
											}
											else
											{
												Result=true;
											}
										}
										if(Result)
										{
											{
												TextColor color(FOREGROUND_GREEN|FOREGROUND_INTENSITY);
												mprintf(L"OK\n");
											}
											if (!InstallCorrection(MInfo[i].ModuleName))
											{
												TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
												mprintf(L"Warning: %s incorrectly installed\n",arc);
											}
											if (!(MInfo[i].Flags&NEW) && MInfo[i].Guid!=FarGuid)
												DeleteFile(BakName);
											if ((!ipc.opt.SavePlugAfterInstall&&i) || (!ipc.opt.SaveFarAfterInstall&&i==0))
												DeleteFile(local_arc);
											// �������� � ���
											else if (!(MInfo[i].Flags&STD))
											{
												// ������ � ��������� �� ������, ���� ����...
												if ((ipc.opt.SavePlugAfterInstall==2&&i) || (ipc.opt.SaveFarAfterInstall==2&&i==0))
												{
													wchar_t Guid[37],Buf[512],Buf2[512];
													GuidToStr(MInfo[i].Guid,Guid);
													GetPrivateProfileString(Guid,ipc.opt.Mode==MODE_UNDO?L"Cur":L"Undo",L"",Buf,ARRAYSIZE(Buf),ipc.Cache);
													GetPrivateProfileString(Guid,ipc.opt.Mode==MODE_UNDO?L"Undo":L"Cur",L"",Buf2,ARRAYSIZE(Buf2),ipc.Cache);
													if (Buf[0])
													{
														wchar_t *p=StrPBrk(Buf,L"|");
														if (p) *p=0;
														if (StrCmpN(Buf,Buf2,lstrlen(Buf)))
														{
															wchar_t arc[MAX_PATH];
															lstrcat(lstrcpy(arc,ipc.TempDirectory),Buf);
															DeleteFile(arc);
														}
													}
												}
												if (!WriteCache(&MInfo[i]))
												{
													TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
													mprintf(L"Error writing to cache %s\n",MInfo[i].New.ArcName);
												}
											}

											bPreInstall=false;
											// ������� ���������
											Exec(bPreInstall,&MInfo[i].Guid,local_config,destpath);
											// � ������ ����������
											Exec(bPreInstall,&MInfo[i].Guid,ipc.Config,destpath);
										}
										else
										{
											TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
											mprintf(L"error\n");
											if (!(MInfo[i].Flags&NEW) && MInfo[i].Guid!=FarGuid)
											{
												CopyFile(BakName,MInfo[i].ModuleName,FALSE);
												DeleteFile(BakName);
											}
										}
									}
								}
							}
						}
						FreeLibrary(hSevenZip);
						if (bDelSevenZip)
							DeleteFile(SevenZip);

						bPreInstall=false;
						Exec(bPreInstall,nullptr,ipc.Config,CurDirectory);
					}
					if(Pos!=-1)
					{
						mi.wID=SC_CLOSE;
						SetMenuItemInfo(hMenu,Pos,MF_BYPOSITION,&mi);
						DrawMenuBar(GetConsoleWindow());
					}
					TextColor color(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
					mprintf(L"\n%-60s",L"Starting Far Manager...");
					STARTUPINFO si={sizeof(si)};
					PROCESS_INFORMATION pi;
					wchar_t FarCmd[2048];
					lstrcpy(FarCmd,MInfo[0].ModuleName);
					lstrcat(FarCmd,L" ");
					lstrcat(FarCmd,ipc.FarParams);
					if(CreateProcess(nullptr,FarCmd,nullptr,nullptr,TRUE,0,nullptr,nullptr,&si,&pi))
					{
						TextColor color(FOREGROUND_GREEN|FOREGROUND_INTENSITY);
						mprintf(L"OK");
					}
					else
					{
						TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
						mprintf(L"Error %d",GetLastError());
					}
					mprintf(L"\n");
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
				}
				free(MInfo);
			}
		}
	}
	LocalFree(argv);
	Clean();
}
