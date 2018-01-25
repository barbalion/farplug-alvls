/****************************************************************************
 * VisRen.hpp
 *
 * Plugin module for FAR Manager 2.0
 *
 * Copyright (c) 2007-2010 Alexey Samlyukov
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
#include "VisRenLng.hpp"        // ����� �������� ��� ���������� ����� �� .lng �����

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
 * ������� ��� ��������������
 ****************************************************************************/
struct File
{
	string strSrcFileName;                         //   ����� ������������ ����� � ������
	string strDestFileName;                        //   �������� �����
	FILETIME ftLastWriteTime;                      //   ����� ��������� �����������

	File()
	{
		strSrcFileName.clear();
		strDestFileName.clear();
		ftLastWriteTime.dwLowDateTime=0;
		ftLastWriteTime.dwHighDateTime=0;
	}

	const File& operator=(const File &f)
	{
		if (this != &f)
		{
			strSrcFileName=f.strSrcFileName;
			strDestFileName=f.strDestFileName;
			ftLastWriteTime.dwLowDateTime=f.ftLastWriteTime.dwLowDateTime;
			ftLastWriteTime.dwHighDateTime=f.ftLastWriteTime.dwHighDateTime;
		}
		return *this;
	}
};

/****************************************************************************
 * ������� ��������� �������
 ****************************************************************************/
extern struct Options {
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
	Options()
	{
		UseLastHistory=0;
		lenFileName=0;
		lenName=0;
		lenExt=0;
		lenDestFileName=0;
		lenDestName=0;
		lenDestExt=0;
		CurBorder=0;
		srcCurCol=0;
		destCurCol=0;
		ShowOrgName=0;
		CaseSensitive=1;
		RegEx=0;
		LogRen=1;
		LoadUndo=0;
		Undo=0;
	}
} Opt;

struct StrOptions {
	string MaskName;
	string MaskExt;
	string Search;
	string Replace;
	string WordDiv;
};

/****************************************************************************
 * Undo ��������������
 ****************************************************************************/
extern struct UndoFileName                       // �������� ��� ������ ��������������
{
	wchar_t *Dir;                                  //   ����� � ������� ���������������
	wchar_t **CurFileName;                         //   �����, � ������� ������������� �����
	wchar_t **OldFileName;                         //   �����, ������� ���� � ������ �� ��������������
	int iCount;                                    //   ���-��
} Undo;

/****************************************************************************
 * ������ �������.
 ****************************************************************************/
extern struct DlgSize
{
	// ��������� �������
	bool Full;
	// ����������
	DWORD W;
	DWORD W2;
	DWORD WS;
	DWORD H;
	// �����������������
	DWORD mW;
	DWORD mW2;
	DWORD mWS;
	DWORD mH;

	DlgSize()
	{
		Full=false;
		W=mW=80-2;
		WS=mWS=(80-2)-37;
		W2=mW2=(80-2)/2-2;
		H=mH=25-2;
	}
} DlgSize;

const wchar_t *GetMsg(int MsgId);
void ErrorMsg(DWORD Title, DWORD Body);
bool YesNoMsg(DWORD Title, DWORD Body);
void FreeUndo();
int DebugMsg(wchar_t *msg, wchar_t *msg2 = L" ", unsigned int i = 1000);
