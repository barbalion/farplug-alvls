/****************************************************************************
 * AdvCmp.hpp
 *
 * Plugin module for FAR Manager 2.0
 *
 * Copyright (c) 2006-2011 Alexey Samlyukov
 ****************************************************************************/
/*
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <wchar.h>
#include "plugin.hpp"
#include "farkeys.hpp"
#include "farcolor.hpp"
#include "string.hpp"
#include "libgfl.h"
#include "AdvCmpLng.hpp"        // ����� �������� ��� ���������� ����� �� .lng �����


/// �����! ���������� ������ �������, ���� ������������� �� �������� ������
void * __cdecl malloc(size_t size);
void __cdecl free(void *block);
void * __cdecl realloc(void *block, size_t size);
#ifdef __cplusplus
void * __cdecl operator new(size_t size);
void __cdecl operator delete(void *block);
#endif
/// ������� strncmp() (��� strcmp() ��� n=-1)
inline int __cdecl Strncmp(const wchar_t *s1, const wchar_t *s2, int n=-1)
{
	return CompareString(0,SORT_STRINGSORT,s1,n,s2,n)-2;
}


/****************************************************************************
 * ����� ����������� �������� FAR
 ****************************************************************************/
extern struct PluginStartupInfo Info;
extern struct FarStandardFunctions FSF;


/****************************************************************************
 * ������� ��������� �������
 ****************************************************************************/
extern struct Options {
	int CmpCase,
			CmpSize,
			CmpTime,
			LowPrecisionTime,
			IgnoreTimeZone,
			CmpContents,
			OnlyTimeDiff,
			Partly,
			PartlyFull,
			PartlyKbSize,
			Ignore,
			IgnoreTemplates,
			ProcessSubfolders,
			MaxScanDepth,
			Filter,
			ProcessSelected,
			SkipSubstr,
			IgnoreMissing,
			ProcessTillFirstDiff,
			SelectedNew,
			Cache,
			CacheIgnore,
			ShowMsg,
			Sound,
			TotalProcess,
			Panel,

			ProcessHidden,
			BufSize;
	char *Buf[2];
	wchar_t *Substr;
	HANDLE hCustomFilter;
} Opt;

extern bool bBrokenByEsc;  //����������/���������� �������� ���������?
extern bool bOpenFail;     // ���������� ������� �������/����
extern bool bGflLoaded;    // libgfl311.dll ���������?

// ���������� � ������
extern struct FarPanelInfo {
	struct PanelInfo PInfo;
	HANDLE hPlugin;
	HANDLE hFilter;         // �������� ������ � ������
	bool bTMP;              // Tmp-������? �������������� �����!
	bool bARC;              // ������-�����? �������������� �����!
	bool bCurFile;          // ��� �������� ����?
} LPanel, RPanel;

// ���������� �� ���� ����
extern struct FarWindowsInfo {
	HWND hFarWindow;        // ��������� ���� ����
	SMALL_RECT Con;         // ���������� ������� (������� - {0,0,79,24})
	RECT Win;               // ���������� ���� (����� - {0,0,1280,896})
	int TruncLen;           // ����������� ����� ���������-���������
} WinInfo;

// ����������
extern struct TotalCmpInfo {
	unsigned int Count;            // ���-�� ������������ ���������
	unsigned __int64 CountSize;    // �� ������
	unsigned __int64 CurCountSize; // ������ ������������ ����
	unsigned int Proc;             // ���-�� ������������ ���������
	unsigned __int64 ProcSize;     // ������ �������������
	unsigned __int64 CurProcSize;  // ������ ������������� �� ������������ ����
	unsigned int LDiff;            // ���-�� ������������ �� ����� ������
	unsigned int RDiff;            // ���-�� ������������ �� ������ ������
} CmpInfo;

// �������� ��� ���������
struct DirList {
	wchar_t *Dir;           // �������
	PluginPanelItem *PPI;   // ��������
	int ItemsNumber;        // ���-��
};

// ����� ���������� ���������
enum ResultCmpItemFlag {
	RCIF_EQUAL  = 0x1,      // ����������   |=|
	RCIF_DIFFER = 0x2,      // ������       |?|

	RCIF_LNEW = 0x6,        // ����� �����  |>|
	RCIF_RNEW = 0xA,        // ������ ����� |<|
};

// ������� ��� ������ � ������� �����������
struct File
{
	string strFileName;
	string strLDir;
	string strRDir;
	DWORD dwAttributes;
	unsigned __int64 nLFileSize;
	unsigned __int64 nRFileSize;
	FILETIME ftLLastWriteTime;
	FILETIME ftRLastWriteTime;
	DWORD dwFlags;

	File()
	{
		strFileName.clear();
		strLDir.clear();
		strRDir.clear();
		dwAttributes=0;
		nLFileSize=0;
		nRFileSize=0;
		ftLLastWriteTime.dwLowDateTime=0;
		ftLLastWriteTime.dwHighDateTime=0;
		ftRLastWriteTime.dwLowDateTime=0;
		ftRLastWriteTime.dwHighDateTime=0;
		dwFlags=0;
	}

	const File& operator=(const File &f)
	{
		if (this != &f)
		{
			strFileName=f.strFileName;
			strLDir=f.strLDir;
			strRDir=f.strRDir;
			dwAttributes=f.dwAttributes;
			nLFileSize=f.nLFileSize;
			nRFileSize=f.nRFileSize;
			ftLLastWriteTime.dwLowDateTime=f.ftLLastWriteTime.dwLowDateTime;
			ftLLastWriteTime.dwHighDateTime=f.ftLLastWriteTime.dwHighDateTime;
			ftRLastWriteTime.dwLowDateTime=f.ftRLastWriteTime.dwLowDateTime;
			ftRLastWriteTime.dwHighDateTime=f.ftRLastWriteTime.dwHighDateTime;
			dwFlags=f.dwFlags;
		}
		return *this;
	}
};

/****************************************************************************
 * ��� ��������� "�� �����������"
 ****************************************************************************/
// ��������� ��������� 2-� ���������
struct ResultCmpItem {
	DWORD   dwFullFileName[2];
	DWORD64 dwWriteTime[2];
	DWORD   dwFlags;
};

// ���
extern struct CacheCmp {
	ResultCmpItem *RCI;
	int ItemsNumber;
} Cache;

const wchar_t *GetMsg(int MsgId);
void ErrorMsg(DWORD Title, DWORD Body);
bool YesNoMsg(DWORD Title, DWORD Body);
int DebugMsg(wchar_t *msg, wchar_t *msg2 = L" ", unsigned int i = 1000);

/****************************************************************************
 *  VisComp.dll
 ****************************************************************************/
typedef int (WINAPI *PCOMPAREFILES)(wchar_t *FileName1, wchar_t *FileName2, DWORD Options);

extern PCOMPAREFILES pCompareFiles;

/****************************************************************************
 *  libgfl311.dll
 ****************************************************************************/
typedef GFL_ERROR		(WINAPI *PGFLLIBRARYINIT)(void);
typedef void				(WINAPI *PGFLENABLELZW)(GFL_BOOL);
typedef void				(WINAPI *PGFLLIBRARYEXIT)(void);
typedef GFL_ERROR		(WINAPI *PGFLLOADBITMAPW)(const wchar_t* filename, GFL_BITMAP** bitmap, const GFL_LOAD_PARAMS* params, GFL_FILE_INFORMATION* info);
typedef GFL_INT32		(WINAPI *PGFLGETNUMBEROFFORMAT)(void);
typedef GFL_ERROR		(WINAPI *PGFLGETFORMATINFORMATIONBYINDEX)(GFL_INT32 index, GFL_FORMAT_INFORMATION* info); 
typedef void				(WINAPI *PGFLGETDEFAULTLOADPARAMS)(GFL_LOAD_PARAMS *);
typedef GFL_ERROR		(WINAPI *PGFLCHANGECOLORDEPTH)(GFL_BITMAP* src, GFL_BITMAP** dst, GFL_MODE mode, GFL_MODE_PARAMS params);
typedef GFL_ERROR		(WINAPI *PGFLROTATE)(GFL_BITMAP* src, GFL_BITMAP** dst, GFL_INT32 angle, const GFL_COLOR *background_color); 
typedef GFL_ERROR		(WINAPI *PGFLRESIZE)(GFL_BITMAP *, GFL_BITMAP **, GFL_INT32, GFL_INT32, GFL_UINT32, GFL_UINT32);
typedef void				(WINAPI *PGFLFREEBITMAP)(GFL_BITMAP *);
typedef void				(WINAPI *PGFLFREEFILEINFORMATION)(GFL_FILE_INFORMATION* info);

extern PGFLLIBRARYINIT pGflLibraryInit;
extern PGFLENABLELZW pGflEnableLZW;
extern PGFLLIBRARYEXIT pGflLibraryExit;
extern PGFLLOADBITMAPW pGflLoadBitmapW;
extern PGFLGETNUMBEROFFORMAT pGflGetNumberOfFormat;
extern PGFLGETFORMATINFORMATIONBYINDEX pGflGetFormatInformationByIndex;
extern PGFLGETDEFAULTLOADPARAMS pGflGetDefaultLoadParams;
extern PGFLCHANGECOLORDEPTH pGflChangeColorDepth;
extern PGFLROTATE pGflRotate;
extern PGFLRESIZE pGflResize;
extern PGFLFREEBITMAP pGflFreeBitmap;
extern PGFLFREEFILEINFORMATION pGflFreeFileInformation;
