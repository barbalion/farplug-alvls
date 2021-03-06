/****************************************************************************
 * AdvCmpProc.hpp
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
#include "DList.hpp"
#include "AdvCmp.hpp"

// ���� ����������� :)
class AdvCmpProc
{
		HANDLE hScreen;
		HANDLE hConInp;    // ����� ������. �����
		bool bStartMsg;    // ����� ���������? ��� ���� ShowMessage()

		bool TitleSaved;
		string strFarTitle;

		// ��������������� ������ ���������� �� �������� DirList.PPI
		struct ItemsIndex {
			PluginPanelItem **pPPI; // ��������
			int iCount;             // ���-��
		};

		// ������ ���������, ��� ������� � ������������ ���������
		DList<File> FileList;

	private:
		bool CheckForEsc(void);
		bool GetFarTitle(string &strTitle);
		void TrunCopy(wchar_t *Dest, const wchar_t *Src, bool bDir, const wchar_t *Msg);
		wchar_t *itoaa(__int64 num, wchar_t *buf);
		void strcentr(wchar_t *Dest, const wchar_t *Src, int len, wchar_t sym);
		void ProgressLine(wchar_t *Dest, unsigned __int64 nCurrent, unsigned __int64 nTotal);
		void ShowMessage(const wchar_t *Dir1, const wchar_t *Name1, const wchar_t *Dir2, const wchar_t *Name2, bool bRedraw);

		void WFD2FFD(WIN32_FIND_DATA &wfd, FAR_FIND_DATA &ffd);
		int GetDirList(const wchar_t *Dir, int ScanDepth, bool OnlyInfo, struct DirList *pList=0);
		int GetDirList(const wchar_t *Dir, struct DirList *pList);
		void FreeDirList(struct DirList *pList);

		inline bool IsNewLine(int c) {return (c == '\r' || c == '\n');}
		inline bool myIsSpace(int c) {return (c == ' ' || c == '\t' || c == '\v' || c == '\f');}
		inline bool IsWhiteSpace(int c) {return (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n');}

		bool mySetFilePointer(HANDLE hf, __int64 distance, DWORD MoveMethod);
		DWORD ProcessCRC(void *pData, register int iLen, DWORD FileCRC);
		int GetCacheResult(DWORD FullFileName1, DWORD FullFileName2, DWORD64 WriteTime1, DWORD64 WriteTime2);
		bool SetCacheResult(DWORD FullFileName1, DWORD FullFileName2, DWORD64 WriteTime1, DWORD64 WriteTime2, DWORD dwFlag);
		bool CompareFiles(const wchar_t *LDir, const PluginPanelItem *pLPPI, const wchar_t *RDir, const PluginPanelItem *pRPPI, int ScanDepth);

		bool CheckScanDepth(const wchar_t *FileName, int ScanDepth);
		bool BuildItemsIndex(bool bLeftPanel,const struct DirList *pList,struct ItemsIndex *pIndex,int ScanDepth);
		void FreeItemsIndex(struct ItemsIndex *pIndex);
		bool BuildFileList(const wchar_t *LDir,const PluginPanelItem *pLPPI,const wchar_t *RDir,const PluginPanelItem *pRPPI,DWORD dwFlag);

	public:
		AdvCmpProc();
		~AdvCmpProc();

		bool CompareDirs(const struct DirList *pLList,const struct DirList *pRList,bool bCompareAll,int ScanDepth);
		int ShowCmpDialog(const struct DirList *pLList,const struct DirList *pRList);
};
