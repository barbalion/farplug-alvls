/****************************************************************************
 * VisRenDlg.cpp
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

#pragma hdrstop
#include "VisRenDlg.hpp"

/****************************************************************************
 **************************** ShowDialog functions **************************
 ****************************************************************************/

/****************************************************************************
 * ID-��������� �������
 ****************************************************************************/
enum {
	DlgBORDER = 0,    // 0
	DlgSIZEICON,      // 1
	DlgLMASKNAME,     // 2
	DlgEMASKNAME,     // 3
	DlgLTEMPL,        // 4
	DlgETEMPLNAME,    // 5
	DlgBTEMPLNAME,    // 6

	DlgLMASKEXT,      // 7
	DlgEMASKEXT,      // 8
	DlgETEMPLEXT,     // 9
	DlgBTEMPLEXT,     //10

	DlgSEP1,          //11
	DlgLSEARCH,       //12
	DlgESEARCH,       //13
	DlgLREPLACE,      //14
	DlgEREPLACE,      //15
	DlgCASE,          //16
	DlgREGEX,         //17

	DlgSEP2,          //18
	DlgLIST,          //19
	DlgSEP3_LOG,      //20
	DlgREN,           //21
	DlgUNDO,          //22
	DlgEDIT,          //23
	DlgCANCEL         //24
};

/***************************************************************************
 * ������ � ��������� ������� ��� �������
 ***************************************************************************/
void VisRenDlg::GetDlgSize()
{
	HANDLE hConOut = CreateFileW(L"CONOUT$", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	CONSOLE_SCREEN_BUFFER_INFO csbiNfo;
	if (GetConsoleScreenBufferInfo(hConOut, &csbiNfo))
	{
		// ���������� ����� ������ FAR: drkns 22.05.2010 20:00:00 +0200 - build 1564
		csbiNfo.dwSize.X=csbiNfo.srWindow.Right-csbiNfo.srWindow.Left+1;
		csbiNfo.dwSize.Y=csbiNfo.srWindow.Bottom-csbiNfo.srWindow.Top+1;

		if (csbiNfo.dwSize.X-2 != DlgSize.mW)
		{
			DlgSize.mW=csbiNfo.dwSize.X-2;
			DlgSize.mWS=DlgSize.mW-37;
			DlgSize.mW2=(DlgSize.mW)/2-2;
		}
		if (csbiNfo.dwSize.Y-2 != DlgSize.mH) DlgSize.mH=csbiNfo.dwSize.Y-2;

	}
	CloseHandle(hConOut);
}

/****************************************************************************
 * ������ ������������ ����� ������, ����� ����� � ����� ����������
 ****************************************************************************/
void VisRenDlg::LenItems(const wchar_t *FileName, bool bDest)
{
	const wchar_t *start=FileName;
	while (*FileName++)
		;
	const wchar_t *end=FileName-1;

	if (bDest)
		Opt.lenDestFileName=max(Opt.lenDestFileName, end-start);
	else
		Opt.lenFileName=max(Opt.lenFileName, end-start);

	while (--FileName != start && *FileName != L'.')
		;
	if (*FileName == L'.')
		(bDest?Opt.lenDestExt:Opt.lenExt)=max((bDest?Opt.lenDestExt:Opt.lenExt), end-FileName-1);
	if (FileName != start)
		(bDest?Opt.lenDestName:Opt.lenName)=max((bDest?Opt.lenDestName:Opt.lenName), FileName-start);
}

/***************************************************************************
 * ���������/���������� ����� ������ � �������
 ***************************************************************************/
bool VisRenDlg::UpdateFarList(HANDLE hDlg, bool bFull, bool bUndoList)
{
	Opt.lenFileName=Opt.lenName=Opt.lenExt=0;
	Opt.lenDestFileName=Opt.lenDestName=Opt.lenDestExt=0;
	// ������ �������
	int widthSrc, widthDest;
	widthSrc=widthDest=(bFull?DlgSize.mW2:DlgSize.W2)-2;
	if (Opt.CurBorder!=0) { widthSrc+=Opt.CurBorder; widthDest-=Opt.CurBorder; }
	// ���������� �����
	int ItemsNumber=(bUndoList?Undo.iCount:FileList.Count());
	// ��� ����������� ������ ������� ����� ���������
	int AddNumber=((bFull?DlgSize.mH:DlgSize.H)-4)-9+1;
	if (ItemsNumber<AddNumber) AddNumber-=ItemsNumber;
	else AddNumber=0;
	// �������� ����������
	FarListInfo ListInfo;
	Info.SendDlgMessage(hDlg,DM_LISTINFO,DlgLIST,(LONG_PTR)&ListInfo);

	if (ListInfo.ItemsNumber)
		Info.SendDlgMessage(hDlg,DM_LISTDELETE,DlgLIST,0);

	wchar_t *buf=(wchar_t *)malloc(DlgSize.mW*sizeof(wchar_t));

	for (int i=0; i<ItemsNumber; i++)
	{
		File *cur=NULL; int index;
		for (cur=FileList.First(), index=0; cur && index<i; cur=FileList.Next(cur), index++)
			;
		const wchar_t *src=NULL,*dest=NULL;

		if (bUndoList)
		{
			src=Undo.CurFileName[i]; dest=Undo.OldFileName[i];
		}
		else
		{
			if (cur)
			{
				src=cur->strSrcFileName.get(); dest=cur->strDestFileName.get();
			}
/*
			else //�� ������ ������
			{
				AddNumber+=ItemsNumber-(i+1);
				ItemsNumber=i+1;
				break;
			}
*/
		}
		// �������, ��� �� ��������� max ����� :)
		LenItems(src, 0); LenItems(dest, 1);

		int lenSrc=wcslen(src), posSrc=Opt.srcCurCol;
		if (lenSrc<=widthSrc) posSrc=0;
		else if (posSrc>lenSrc-widthSrc) posSrc=lenSrc-widthSrc;

		int lenDest=wcslen(dest), posDest=Opt.destCurCol;
		if (lenDest<=widthDest) posDest=0;
		else if (posDest>lenDest-widthDest) posDest=lenDest-widthDest;

		FSF.sprintf( buf, L"%-*.*s%c%-*.*s", widthSrc, widthSrc, src+posSrc,
								 0x2502, widthDest, widthDest, (bError?GetMsg(MError):dest+posDest) );
		Info.SendDlgMessage(hDlg,DM_LISTADDSTR,DlgLIST,(LONG_PTR)buf);
	}

	for (int i=ItemsNumber; i<ItemsNumber+AddNumber; i++)
	{
		FSF.sprintf( buf, L"%-*.*s%c%-*.*s", widthSrc, widthSrc, L"",
								 0x2502, widthDest, widthDest, L"" );
		Info.SendDlgMessage(hDlg,DM_LISTADDSTR,DlgLIST,(LONG_PTR)buf);
	}

	if (buf) free(buf);
	// ����������� ��������� �������
	FarListPos ListPos;
	ListPos.SelectPos=ListInfo.SelectPos<ItemsNumber?ListInfo.SelectPos:(ListInfo.SelectPos-1<0?0:ListInfo.SelectPos-1);
	static bool bOldFull=bFull;
	if (bOldFull!=bFull) { ListPos.TopPos=-1; bOldFull=bFull; }
	else ListPos.TopPos=ListInfo.TopPos;
	Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,DlgLIST,(LONG_PTR)&ListPos);

	Info.SendDlgMessage(hDlg,DM_LISTSETMOUSEREACTION,DlgLIST,(LONG_PTR)LMRT_NEVER);
	return true;
}

/***************************************************************************
 * ��������� ������� �������
 ***************************************************************************/
void VisRenDlg::DlgResize(HANDLE hDlg, bool bF5)
{
	COORD c;
	if (bF5) // ������ F5
	{
		if (DlgSize.Full==false)  // ��� ���������� ������
		{
			c.X=DlgSize.mW; c.Y=DlgSize.mH;
			DlgSize.Full=true;  // ���������� ������������
		}
		else
		{
			c.X=DlgSize.W; c.Y=DlgSize.H;
			DlgSize.Full=false; // ������� ����������
		}
	}
	else // ����� ������ ����������� ������� �������
	{
		if (DlgSize.Full) { c.X=DlgSize.mW; c.Y=DlgSize.mH; }
		else { c.X=DlgSize.W; c.Y=DlgSize.H; }
	}
	Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
	Opt.srcCurCol=Opt.destCurCol=Opt.CurBorder=0;

	for (int i=DlgBORDER; i<=DlgCANCEL; i++)
	{
		FarDialogItem *Item=(FarDialogItem *)malloc(Info.SendDlgMessage(hDlg,DM_GETDLGITEM,i,0));
		if (!Item) return;
		Info.SendDlgMessage(hDlg, DM_GETDLGITEM, i, (LONG_PTR)Item);
		switch (i)
		{
			case DlgBORDER:
				Item->X2=(DlgSize.Full?DlgSize.mW:DlgSize.W)-1;
				Item->Y2=(DlgSize.Full?DlgSize.mH:DlgSize.H)-1;
				break;
				case DlgSIZEICON:
				Item->X1=(DlgSize.Full?DlgSize.mW:DlgSize.W)-4;
				break;
			case DlgEMASKNAME:
				Item->X2=(DlgSize.Full?DlgSize.mWS:DlgSize.WS)-3;
				break;
			case DlgLMASKEXT:
			case DlgCASE:
			case DlgREGEX:
				Item->X1=(DlgSize.Full?DlgSize.mWS:DlgSize.WS);
				break;
			case DlgEMASKEXT:
				Item->X1=(DlgSize.Full?DlgSize.mWS:DlgSize.WS);
				Item->X2=(DlgSize.Full?DlgSize.mW:DlgSize.W)-3;
				break;
			case DlgETEMPLEXT:
				Item->X1=(DlgSize.Full?DlgSize.mWS:DlgSize.WS);
				Item->X2=(DlgSize.Full?DlgSize.mW:DlgSize.W)-8;
				break;
			case DlgBTEMPLEXT:
				Item->X1=(DlgSize.Full?DlgSize.mW:DlgSize.W)-5;
				break;
			case DlgESEARCH:
			case DlgEREPLACE:
				Item->X2=(DlgSize.Full?DlgSize.mWS:DlgSize.WS)-3;
				break;
			case DlgLIST:
			{
				Item->X2=(DlgSize.Full?DlgSize.mW:DlgSize.W)-2;
				Item->Y2=(DlgSize.Full?DlgSize.mH:DlgSize.H)-4;

				// !!! ������� ����������� FARa �� �������� ListItems �� FarDialogItem
				UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo);
				break;
			}
			case DlgSEP3_LOG:
				Item->Y1=(DlgSize.Full?DlgSize.mH:DlgSize.H)-3;
				break;
			case DlgREN:
			case DlgUNDO:
			case DlgEDIT:
			case DlgCANCEL:
				Item->Y1=(DlgSize.Full?DlgSize.mH:DlgSize.H)-2;
				break;
		}
		Info.SendDlgMessage(hDlg, DM_SETDLGITEM, i, (LONG_PTR)Item);
		free(Item);
	}
	Info.SendDlgMessage(hDlg, DM_RESIZEDIALOG, 0, (LONG_PTR)&c);
	c.X=c.Y=-1;
	Info.SendDlgMessage(hDlg, DM_MOVEDIALOG, true, (LONG_PTR)&c);
	Info.SendDlgMessage(hDlg, DM_LISTSETMOUSEREACTION, DlgLIST, (LONG_PTR)LMRT_NEVER);
	Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
	return;
}

/****************************************************************************
 * ������������ ������ ����� �� ���������� �������
 ****************************************************************************/
bool VisRenDlg::SetMask(HANDLE hDlg, DWORD IdMask, DWORD IdTempl)
{
	wchar_t templ[15];
	FarListPos ListPos;
	Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, IdTempl, (LONG_PTR)&ListPos);

	if (IdTempl==DlgETEMPLNAME) // ������ � ��������� �����
	{
		switch (ListPos.SelectPos)
		{
			case 0:  wcscpy(templ, L"[N]");      break;
			case 1:  FSF.sprintf(templ, L"[N1-%d]", Opt.lenName); break;
			case 2:  wcscpy(templ, L"[C1+1]");   break;
			//---
			case 4:  wcscpy(templ, L"[L]");      break;
			case 5:  wcscpy(templ, L"[U]");      break;
			case 6:  wcscpy(templ, L"[F]");      break;
			case 7:  wcscpy(templ, L"[T]");      break;
			case 8:  wcscpy(templ, L"[M]");      break;
			//---
			case 10: wcscpy(templ, L"[#]");      break;
			case 11: wcscpy(templ, L"[t]");      break;
			case 12: wcscpy(templ, L"[a]");      break;
			case 13: wcscpy(templ, L"[l]");      break;
			case 14: wcscpy(templ, L"[y]");      break;
			case 15: wcscpy(templ, L"[g]");      break;
			//---
			case 17: wcscpy(templ, L"[c]");      break;
			case 18: wcscpy(templ, L"[m]");      break;
			case 19: wcscpy(templ, L"[d]");      break;
			case 20: wcscpy(templ, L"[r]");      break;
			//---
			case 22: wcscpy(templ, L"[DM]");     break;
			case 23: wcscpy(templ, L"[TM]");     break;
			case 24: wcscpy(templ, L"[TL]");     break;
			case 25: wcscpy(templ, L"[TR]");     break;
		}
	}
	else   // ������ � ��������� ����������
	{
		switch (ListPos.SelectPos)
		{
			case 0:  wcscpy(templ, L"[E]");      break;
			case 1:  FSF.sprintf(templ, L"[E1-%d]", Opt.lenExt); break;
			case 2:  wcscpy(templ, L"[C1+1]");   break;
			//---
			case 4:  wcscpy(templ, L"[L]");      break;
			case 5:  wcscpy(templ, L"[U]");      break;
			case 6:  wcscpy(templ, L"[F]");      break;
			case 7:  wcscpy(templ, L"[T]");      break;
		}
	}

	COORD Pos;
	Info.SendDlgMessage(hDlg, DM_GETCURSORPOS, IdMask, (LONG_PTR)&Pos);
	EditorSelect es;
	Info.SendDlgMessage(hDlg, DM_GETSELECTION, IdMask, (LONG_PTR)&es);
	string strBuf((const wchar_t*)Info.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, IdMask, 0));
	size_t length=strBuf.length();
	string strBuf2;

	if (es.BlockType!=BTYPE_NONE && es.BlockStartPos>=0)
	{
		Pos.X=es.BlockStartPos;
		strBuf2=(strBuf.get()+es.BlockStartPos+es.BlockWidth);  // ������� �� ����������
		strBuf(strBuf.get(),Pos.X);
		strBuf+=strBuf2.get();
	}

	if (Pos.X>=length)
		strBuf+=templ;
	else
	{
		strBuf2=(strBuf.get()+Pos.X);
		strBuf(strBuf.get(),Pos.X);
		strBuf+=templ;
		strBuf+=strBuf2.get();
	}
	Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, IdMask, (LONG_PTR)FSF.Trim(strBuf.get()));
	Info.SendDlgMessage(hDlg, DM_SETFOCUS, IdMask, 0);
	Pos.X+=wcslen(templ);
	Info.SendDlgMessage(hDlg, DM_SETCURSORPOS, IdMask, (LONG_PTR)&Pos);
	es.BlockType=BTYPE_NONE;
	Info.SendDlgMessage(hDlg, DM_SETSELECTION, IdMask, (LONG_PTR)&es);
	return true;
}

/****************************************************************************
 * ��������� Drag&Drop ����� � ����� ������
 ****************************************************************************/
void VisRenDlg::MouseDragDrop(HANDLE hDlg, DWORD dwMousePosY)
{
	FarListPos ListPos;
	Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
	if (ListPos.SelectPos>=FileList.Count()) return;

	SMALL_RECT dlgRect;
	Info.SendDlgMessage(hDlg, DM_GETDLGRECT, 0, (LONG_PTR)&dlgRect);
	int CurPos=ListPos.TopPos+(dwMousePosY-(dlgRect.Top+9));

	if (CurPos<0) CurPos=0;
	else if (CurPos>=FileList.Count()) CurPos=FileList.Count()-1;

	if (CurPos!=ListPos.SelectPos)
	{
		bool bUp=CurPos<ListPos.SelectPos;
		if (bUp?ListPos.SelectPos>0:ListPos.SelectPos<FileList.Count()-1)
		{
			Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
			File add, *cur=NULL; unsigned index;
			for (cur=FileList.First(), index=0; cur && index<ListPos.SelectPos; cur=FileList.Next(cur), index++)
				;
			if (cur)
			{
				add=*cur;
				cur=FileList.Delete(cur);
				if (bUp)
					FileList.InsertBefore(cur,&add);
				else
				{
					cur=FileList.Next(cur);
					FileList.InsertAfter(cur,&add);
				}
			}
			for (int i=DlgEMASKNAME; i<=DlgEREPLACE; i++)
				switch(i)
				{
					case DlgEMASKNAME: case DlgEMASKEXT:
					case DlgESEARCH:   case DlgEREPLACE:
					{
						FarDialogItem *Item=(FarDialogItem *)malloc(Info.SendDlgMessage(hDlg,DM_GETDLGITEM,i,0));
						if (Item)
						{
							Info.SendDlgMessage(hDlg, DM_GETDLGITEM, i, (LONG_PTR)Item);
							Info.SendDlgMessage(hDlg, DN_EDITCHANGE, i, (LONG_PTR)Item);
							free(Item);
							break;
						}
					}
				}
			bUp?ListPos.SelectPos--:ListPos.SelectPos++;
			Info.SendDlgMessage(hDlg, DM_LISTSETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
			Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
		}
	}
	return;
}

/****************************************************************************
 * ��������� ����� ������ �������� ���� � ����� ������
 ****************************************************************************/
int VisRenDlg::ListMouseRightClick(HANDLE hDlg, DWORD dwMousePosX, DWORD dwMousePosY)
{
	SMALL_RECT dlgRect;
	Info.SendDlgMessage(hDlg, DM_GETDLGRECT, 0, (LONG_PTR)&dlgRect);
	FarListPos ListPos;
	Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
	int Pos=ListPos.TopPos+(dwMousePosY-(dlgRect.Top+9));
	if ( Pos>=(Opt.LoadUndo?Undo.iCount:FileList.Count())
				// �������� �� ��������� ������ DlgLIST
				|| dwMousePosX<=dlgRect.Left  || dwMousePosX>=dlgRect.Right
				|| dwMousePosY<=dlgRect.Top+8 || dwMousePosY>=dlgRect.Bottom-2
		)
		return -1;
	ListPos.SelectPos=Pos;
	Info.SendDlgMessage(hDlg, DM_LISTSETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
	return Pos;
}

/****************************************************************************
 * �-��� ���������� ������� ��� �� ����� ������ �� ��������� �����
 ****************************************************************************/
void VisRenDlg::ShowName(int Pos)
{
	int i;
	wchar_t srcName[4][66], destName[4][66];

	for (i=0; i<4; i++)
	{
		srcName[i][0]=0; destName[i][0]=0;
	}

	File *cur=NULL; int index;
	for (cur=FileList.First(), index=0; cur && index<Pos; cur=FileList.Next(cur), index++)
			;
	// ������ ���
	wchar_t *src=Opt.LoadUndo?Undo.CurFileName[Pos]:cur->strSrcFileName.get();
	int len=wcslen(src);
	for (i=0; i<4; i++, len-=65)
	{
		if (len<65)
		{
			lstrcpyn(srcName[i], src+i*65, len+1);
			break;
		}
		else
			lstrcpyn(srcName[i], src+i*65, 66);
	}
	// ����� ���
	wchar_t *dest=Opt.LoadUndo?Undo.OldFileName[Pos]:cur->strDestFileName.get();
	len=wcslen(dest);
	for (i=0; i<4; i++, len-=65)
	{
		if (len<65)
		{
			lstrcpyn(destName[i], dest+i*65, len+1);
			break;
		}
		else
			lstrcpyn(destName[i], dest+i*65, 66);
	}

	struct FarDialogItem DialogItems[] = {
		//			Type	X1	Y1	X2	Y2	Focus	Selected/History	Flags	DefaultButton	PtrData	MaxLen
		/* 0*/{DI_DOUBLEBOX,0, 0,70,14, 0, 0,                    0, 0, GetMsg(MFullFileName), 0},
		/* 1*/{DI_SINGLEBOX,2, 1,68, 6, 0, 0,         DIF_LEFTTEXT, 0, GetMsg(MOldName), 0},
		/* 2*/{DI_TEXT,     3, 2, 0, 0, 0, 0,         DIF_LEFTTEXT, 0, srcName[0],0},
		/* 3*/{DI_TEXT,     3, 3, 0, 0, 0, 0,         DIF_LEFTTEXT, 0, srcName[1],0},
		/* 4*/{DI_TEXT,     3, 4, 0, 0, 0, 0,         DIF_LEFTTEXT, 0, srcName[2],0},
		/* 5*/{DI_TEXT,     3, 5, 0, 0, 0, 0,         DIF_LEFTTEXT, 0, srcName[3],0},
		/* 6*/{DI_SINGLEBOX,2, 7,68,12, 0, 0,         DIF_LEFTTEXT, 0, GetMsg(MNewName), 0},
		/* 7*/{DI_TEXT,     3, 8, 0, 0, 0, 0,         DIF_LEFTTEXT, 0, destName[0],0},
		/* 8*/{DI_TEXT,     3, 9, 0, 0, 0, 0,         DIF_LEFTTEXT, 0, destName[1],0},
		/* 9*/{DI_TEXT,     3,10, 0, 0, 0, 0,         DIF_LEFTTEXT, 0, destName[2],0},
		/*10*/{DI_TEXT,     3,11, 0, 0, 0, 0,         DIF_LEFTTEXT, 0, destName[3],0},
		/*11*/{DI_BUTTON,   0,13, 0, 0, 0, 0,      DIF_CENTERGROUP, 1, GetMsg(MOK),0},
	};

	HANDLE hDlg=Info.DialogInit(Info.ModuleNumber,-1,-1,71,15,L"Contents", DialogItems,sizeof(DialogItems)/sizeof(DialogItems[0]),0,FDLG_SMALLDIALOG,0,0);
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		Info.DialogRun(hDlg);
		Info.DialogFree(hDlg);
	}
	return;
}

LONG_PTR WINAPI VisRenDlg::ShowDialogProcThunk(HANDLE hDlg, int Msg, int Param1, LONG_PTR Param2)
{
	VisRenDlg* Class=(VisRenDlg*)Info.SendDlgMessage(hDlg,DM_GETDLGDATA,0,0);
	return Class->ShowDialogProc(hDlg,Msg,Param1,Param2);
}

/****************************************************************************
 * ���������� ������� ��� ShowDialog
 ****************************************************************************/
LONG_PTR WINAPI VisRenDlg::ShowDialogProc(HANDLE hDlg, int Msg, int Param1, LONG_PTR Param2)
{
	switch (Msg)
	{
		case DN_INITDIALOG:
			{
				StrOpt.MaskName.clear();
				StrOpt.MaskExt.clear();
				StrOpt.Search.clear();
				StrOpt.Replace.clear();
				StrOpt.WordDiv=L"-. _&";
				PluginRootKey=Info.RootKey;
				PluginRootKey+=L"\\VisRen";
				HKEY hKey;
				if (RegOpenKeyEx(HKEY_CURRENT_USER,PluginRootKey.get(),0,KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
				{
					DWORD Type, DataSize;
					if (RegQueryValueEx(hKey,L"WordDiv",0,&Type,0,&DataSize) == ERROR_SUCCESS)
					{
						wchar_t *Buf=(wchar_t *)malloc(DataSize/sizeof(wchar_t)+1);
						if (Buf)
						{
							if (RegQueryValueEx(hKey,L"WordDiv",0,&Type,(unsigned char *)Buf,&DataSize) == ERROR_SUCCESS)
								StrOpt.WordDiv=Buf;
							free(Buf);
						}
					}
					RegCloseKey(hKey);
				}

				Opt.CurBorder=Opt.srcCurCol=Opt.destCurCol=0;
				Opt.CaseSensitive=Opt.LogRen=1;
				Opt.RegEx=Opt.LoadUndo=Opt.Undo=0;
				StartPosX=Focus=-1;
				bError=false;
				SaveItemFocus=DlgEMASKNAME;

				Info.SendDlgMessage(hDlg, DM_SETCOMBOBOXEVENT, DlgETEMPLNAME, (LONG_PTR)CBET_KEY);
				Info.SendDlgMessage(hDlg, DM_SETCOMBOBOXEVENT, DlgETEMPLEXT, (LONG_PTR)CBET_KEY);

				if (!Opt.UseLastHistory)
				{
					Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, DlgEMASKNAME, (LONG_PTR)L"[N]");
					Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, DlgEMASKEXT, (LONG_PTR)L"[E]");
				}
				// ��������� ���������� ������ �������
				DlgResize(hDlg);

				// ��� ����������� ������������� ����� �� �������
				FarDialogItem *Item;
				Item=(FarDialogItem *)malloc(Info.SendDlgMessage(hDlg,DM_GETDLGITEM,DlgEMASKNAME,0));
				if (Item)
				{
					Info.SendDlgMessage(hDlg,DM_GETDLGITEM,DlgEMASKNAME,(LONG_PTR)Item);
					Info.SendDlgMessage(hDlg,DN_EDITCHANGE,DlgEMASKNAME,(LONG_PTR)Item);
					free(Item);
				}
				Item=(FarDialogItem *)malloc(Info.SendDlgMessage(hDlg,DM_GETDLGITEM,DlgEMASKEXT,0));
				if (Item)
				{
					Info.SendDlgMessage(hDlg,DM_GETDLGITEM,DlgEMASKEXT,(LONG_PTR)Item);
					Info.SendDlgMessage(hDlg,DN_EDITCHANGE,DlgEMASKEXT,(LONG_PTR)Item);
					free(Item);
				}
				break;
			}

	/************************************************************************/

		case DN_RESIZECONSOLE:
			GetDlgSize();
			DlgResize(hDlg);
			return true;

	/************************************************************************/

		case DN_DRAWDIALOG:
			{
				Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, DlgSIZEICON, (LONG_PTR)(DlgSize.Full?L"[]":L"[]"));
				string buf;
				if (Opt.LoadUndo && Undo.Dir)
				{
					buf=Undo.Dir;
					FSF.TruncStr(buf.get(), DlgSize.Full?DlgSize.mW-6:DlgSize.W-6);
					buf.updsize();
				}
				string sep=L" ";
				sep+=Opt.LoadUndo?buf.get():GetMsg(MSep);
				sep+=L" ";
				Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, DlgSEP2, (LONG_PTR)sep.get());

				sep=L" ";
				sep+=Opt.LogRen?0x221a:L' ';
				sep+=L' ';
				sep+=GetMsg(MCreateLog);
				sep+=Undo.iCount?L"* ":L" ";
				Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, DlgSEP3_LOG, (LONG_PTR)sep.get());

				for (int i=DlgEMASKNAME; i<=DlgEDIT; i++)
				{
					switch (i)
					{
						case DlgEMASKNAME: case DlgETEMPLNAME: case DlgBTEMPLNAME:
						case DlgEMASKEXT:  case DlgETEMPLEXT:  case DlgBTEMPLEXT:
						case DlgESEARCH:   case DlgEREPLACE:   case DlgCASE:
						case DlgREGEX:
							Info.SendDlgMessage(hDlg, DM_ENABLE, i, !(Opt.LoadUndo || !FileList.Count()));
							break;
						case DlgREN:
							Info.SendDlgMessage(hDlg, DM_ENABLE, i, !(!FileList.Count() || bError || (Opt.LoadUndo && !Undo.iCount)));
							break;
						case DlgUNDO:
							Info.SendDlgMessage(hDlg, DM_ENABLE, i, !(!Undo.iCount || Opt.LoadUndo || bError));
							break;
						case DlgEDIT:
							Info.SendDlgMessage(hDlg, DM_ENABLE, i, !(Opt.LoadUndo || bError || !FileList.Count()));
							break;
					}
				}
				return true;
			}

	/************************************************************************/

		case DN_BTNCLICK:
			if (Param1==DlgBTEMPLNAME)
			{
				if (!SetMask(hDlg, DlgEMASKNAME, DlgETEMPLNAME)) return false;
			}
			else if (Param1==DlgBTEMPLEXT)
			{
				if (!SetMask(hDlg, DlgEMASKEXT, DlgETEMPLEXT)) return false;
			}
			else if (Param1==DlgCASE || Param1==DlgREGEX)
			{
				Param1==DlgCASE?Opt.CaseSensitive=Param2:Opt.RegEx=Param2;
				FarDialogItem *Item=(FarDialogItem *)malloc(Info.SendDlgMessage(hDlg,DM_GETDLGITEM,DlgESEARCH,0));
				if (Item)
				{
					Info.SendDlgMessage(hDlg, DM_GETDLGITEM, DlgESEARCH, (LONG_PTR)Item);
					Info.SendDlgMessage(hDlg, DN_EDITCHANGE, DlgESEARCH, (LONG_PTR)Item);
					free(Item);
				}
			}
			else if (Param1==DlgUNDO)
			{
				Opt.srcCurCol=Opt.destCurCol=Opt.CurBorder=0;
				UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo=1);
				Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgREN, 0);
			}
			break;

	/************************************************************************/

		case DN_MOUSECLICK:
			if (Param1==DlgSIZEICON && ((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED)
				goto DLGRESIZE;
			else if (Param1==DlgSEP3_LOG &&
							((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED)
				goto LOGREN;
			else if (Param1==DlgSEP3_LOG &&
							((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==RIGHTMOST_BUTTON_PRESSED)
				goto CLEARLOGREN;
			else if (Param1==DlgLIST &&
							((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
							((MOUSE_EVENT_RECORD *)Param2)->dwEventFlags==DOUBLE_CLICK )
				goto GOTOFILE;
			else if (Param1==DlgLIST &&
							((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==RIGHTMOST_BUTTON_PRESSED)
			{
				Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgLIST, 0);
				goto RIGHTCLICK;
			}
			return false;

	/************************************************************************/

		case DN_GOTFOCUS:
			{
				bool Ret=true;
				switch (Param1)
				{
					case DlgLIST:
						Focus=DlgLIST; break;
					default:
						Ret=false; break;
				}
				/* $ !!  skirda, 10.07.2007 1:27:03:
				*       � ����������� ���� ����� ����� ������ ���� ShowDialog(-1)
				*       � � ����������� ���������� - ShowDialog(OldPos); ShowDialog(FocusPos);
				*
				*       AS,  �.�. ��� ���� ��� ���� ������� "DN_DRAWDIALOG"
				*            � ��� ����� "DN_DRAWDLGITEM"
				*       ����� ���� ����� ��� DN_CTLCOLORDLGITEM:
				*/
				Info.SendDlgMessage(hDlg, DM_SHOWITEM, DlgSEP2, 1);
					/* $ */
				if (Ret) Info.SendDlgMessage(hDlg, DM_SETMOUSEEVENTNOTIFY, 1, 0);
			}
			break;

	/************************************************************************/

		case DN_MOUSEEVENT:
			{
 RIGHTCLICK:
				MOUSE_EVENT_RECORD *MRec=(MOUSE_EVENT_RECORD *)Param2;
				if ( MRec->dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
							MRec->dwEventFlags==MOUSE_MOVED )
				{
					switch (Focus)
					{
						case DlgLIST:
							if (!Opt.LoadUndo)
								MouseDragDrop(hDlg,MRec->dwMousePosition.Y);
							return false;
					}
				}
				// ��������� ����� ������ �������� ���� � ����� ������
				else if (MRec->dwButtonState==RIGHTMOST_BUTTON_PRESSED && Focus==DlgLIST)
				{
					int Pos=ListMouseRightClick(hDlg, MRec->dwMousePosition.X, MRec->dwMousePosition.Y);
					if (Pos>=0)
					{
						ShowName(Pos);
						return false;
					}
				}
				return true;
			}

	/************************************************************************/

		case DN_KILLFOCUS:
			{
				switch (Param1)
					case DlgLIST:
						Focus=-1;
						Info.SendDlgMessage(hDlg, DM_SETMOUSEEVENTNOTIFY, 0, 0);
						break;
			}
			break;

	/************************************************************************/

		case DN_KEY:
			if (Param2==KEY_F1 && (Param1==DlgETEMPLNAME || Param1==DlgETEMPLEXT))
			{
				Info.ShowHelp(Info.ModuleName, 0, 0);
				return true;
			}
			else if (Param2==KEY_F2 && !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
			{
 LOGREN:
				if (Opt.LogRen) Opt.LogRen=0;
				else Opt.LogRen=1;
				Info.SendDlgMessage(hDlg, DM_REDRAW, 0, 0);
				return true;
			}
			//----
			else if (Param2==KEY_F3)
			{
				int Pos=Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, 0);
				if (Pos<(Opt.LoadUndo?Undo.iCount:FileList.Count())) ShowName(Pos);
				return true;
			}
			//----
			else if (Param2==KEY_F4)
			{
				if ( (Param1==DlgETEMPLNAME || Param1==DlgETEMPLEXT) &&
							Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0) )
				{
					if (  Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgETEMPLNAME, 0)==7 ||
								Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgETEMPLEXT, 0)==7)
					{
						wchar_t WordDiv[20];
						if (Info.InputBox(GetMsg(MWordDivTitle), GetMsg(MWordDivBody), L"VisRenWordDiv", StrOpt.WordDiv.get(),
															WordDiv,19, 0, FIB_BUTTONS ))
						{
							StrOpt.WordDiv=WordDiv;
							HKEY hKey; DWORD Disposition;
							if (RegCreateKeyEx(HKEY_CURRENT_USER,PluginRootKey,0,0,0,KEY_ALL_ACCESS,0,&hKey,&Disposition) == ERROR_SUCCESS)
							{
								RegSetValueEx(hKey,L"WordDiv",0,REG_SZ,(unsigned char *)WordDiv,(int)(wcslen(WordDiv)+1)*sizeof(wchar_t));
								RegCloseKey(hKey);
							}
						}
					}
				}
				else if (!Opt.LoadUndo)
					Info.SendDlgMessage(hDlg, DM_CLOSE, DlgEDIT, 0);
				return true;
			}
			//----
			else if (Param2==KEY_F5 && !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
			{
 DLGRESIZE:
				DlgResize(hDlg, true);  //true - �.�. ������ F5
				return true;
			}
			//----
			else if (Param2==KEY_F6 && Undo.iCount && !bError &&
							!Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
			{
				Opt.srcCurCol=Opt.destCurCol=Opt.CurBorder=0;
				UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo=1);
				Info.SendDlgMessage(hDlg, DM_REDRAW, 0, 0);
				Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgREN, 0);
				return true;
			}
			//----
			else if (Param2==KEY_F8 && !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
			{
 CLEARLOGREN:
				if (Undo.iCount && !Opt.LoadUndo && YesNoMsg(MClearLogTitle, MClearLogBody))
				{
					FreeUndo();
					int ItemFocus=Info.SendDlgMessage(hDlg, DM_GETFOCUS, 0, 0);
					Info.SendDlgMessage(hDlg, DM_REDRAW, 0, 0);
					Info.SendDlgMessage(hDlg, DM_SETFOCUS, (ItemFocus!=DlgUNDO?ItemFocus:DlgREN), 0);
					return true;
				}
				return false;
			}
			//----
			else if (Param2==KEY_F12 && !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
			{
				int ItemFocus=Info.SendDlgMessage(hDlg, DM_GETFOCUS, 0, 0);
				if (ItemFocus!=DlgLIST) SaveItemFocus=ItemFocus;
				Info.SendDlgMessage(hDlg, DM_SETFOCUS, ItemFocus!=DlgLIST?DlgLIST:SaveItemFocus, 0);
				return true;
			}
			//----
			else if (Param2==KEY_INS)
			{
				if (Param1==DlgETEMPLNAME || Param1==DlgBTEMPLNAME)
				{
					if (SetMask(hDlg, DlgEMASKNAME, DlgETEMPLNAME))
					{
						if (Param1==DlgETEMPLNAME)
							Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgETEMPLNAME, 0);
						return true;
					}
				}
				else if (Param1==DlgETEMPLEXT || Param1==DlgBTEMPLEXT)
				{
					if (SetMask(hDlg, DlgEMASKEXT, DlgETEMPLEXT))
					{
						if (Param1==DlgETEMPLEXT)
							Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgETEMPLEXT, 0);
						return true;
					}
				}
			}
			//----
			else if (Param2==KEY_DEL && Param1==DlgLIST)
			{
				int Pos=Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, 0);
				if (Pos>=(Opt.LoadUndo?Undo.iCount:FileList.Count())) return false;
				Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
				FarListPos ListPos={Pos, -1};
				FarListDelete ListDel={Pos, 1};
				Info.SendDlgMessage(hDlg, DM_LISTDELETE, DlgLIST, (LONG_PTR)&ListDel);

				if (Opt.LoadUndo)
				{
					do
					{
						Undo.CurFileName[Pos]=(wchar_t*)realloc(Undo.CurFileName[Pos],(wcslen(Undo.CurFileName[Pos+1])+1)*sizeof(wchar_t));
						Undo.OldFileName[Pos]=(wchar_t*)realloc(Undo.OldFileName[Pos],(wcslen(Undo.OldFileName[Pos+1])+1)*sizeof(wchar_t));
						wcscpy(Undo.CurFileName[Pos], Undo.CurFileName[Pos+1]);
						wcscpy(Undo.OldFileName[Pos], Undo.OldFileName[Pos+1]);
					} while(++Pos<Undo.iCount-1);
					free(Undo.CurFileName[Pos]); Undo.CurFileName[Pos]=0;
					free(Undo.OldFileName[Pos]); Undo.OldFileName[Pos]=0;
					--Undo.iCount;
				}
				else
				{
					File *cur=NULL; unsigned index;
					for (cur=FileList.First(), index=0; cur && index<Pos; cur=FileList.Next(cur), index++)
						;
					if (cur) FileList.Delete(cur);
				}

				Info.SendDlgMessage(hDlg, DM_LISTSETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
				UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo);
				Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
				return true;
			}
			//----
			else if ( Param1==DlgLIST && !Opt.LoadUndo &&
								(Param2==(KEY_CTRL|KEY_UP) || Param2==(KEY_RCTRL|KEY_UP) ||
								 Param2==(KEY_CTRL|KEY_DOWN) || Param2==(KEY_RCTRL|KEY_DOWN)) )
			{
				FarListPos ListPos;
				Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
				if (ListPos.SelectPos>=FileList.Count()) 
					return false;
				bool bUp=(Param2==(KEY_CTRL|KEY_UP) || Param2==(KEY_RCTRL|KEY_UP));
				if (bUp?ListPos.SelectPos>0:ListPos.SelectPos<FileList.Count()-1)
				{
					Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
					File add, *cur=NULL; unsigned index;
					for (cur=FileList.First(), index=0; cur && index<ListPos.SelectPos; cur=FileList.Next(cur), index++)
						;
					if (cur)
					{
						add=*cur;
						cur=FileList.Delete(cur);
						if (bUp)
							FileList.InsertBefore(cur,&add);
						else
						{
							cur=FileList.Next(cur);
							FileList.InsertAfter(cur,&add);
						}
					}
					for (int i=DlgEMASKNAME; i<=DlgEREPLACE; i++)
						switch(i)
						{
							case DlgEMASKNAME: case DlgEMASKEXT:
							case DlgESEARCH:   case DlgEREPLACE:
							{
								FarDialogItem *Item=(FarDialogItem *)malloc(Info.SendDlgMessage(hDlg,DM_GETDLGITEM,i,0));
								if (Item)
								{
									Info.SendDlgMessage(hDlg, DM_GETDLGITEM, i, (LONG_PTR)Item);
									Info.SendDlgMessage(hDlg, DN_EDITCHANGE, i, (LONG_PTR)Item);
									free(Item);
									break;
								}
							}
						}
					bUp?ListPos.SelectPos--:ListPos.SelectPos++;
					Info.SendDlgMessage(hDlg, DM_LISTSETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
					Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
					return true;
				}
			}
			//----
			else if ( Param1==DlgLIST &&
								(Param2==(KEY_CTRL|KEY_LEFT) || Param2==(KEY_RCTRL|KEY_LEFT) ||
								Param2==(KEY_CTRL|KEY_RIGHT) || Param2==(KEY_RCTRL|KEY_RIGHT) ||
								Param2==(KEY_CTRL|KEY_NUMPAD5) || Param2==(KEY_RCTRL|KEY_NUMPAD5)) )
			{
				bool bLeft=(Param2==(KEY_CTRL|KEY_LEFT) || Param2==(KEY_RCTRL|KEY_LEFT));
				int maxBorder=(DlgSize.Full?DlgSize.mW2:DlgSize.W2)-2-5;
				bool Ret=false;
				if (Param2==(KEY_CTRL|KEY_NUMPAD5) || Param2==(KEY_RCTRL|KEY_NUMPAD5))
				{
					Opt.srcCurCol=Opt.destCurCol=Opt.CurBorder=0; Ret=true;
				}
				else if (bLeft?Opt.CurBorder>-maxBorder:Opt.CurBorder<maxBorder)
				{
					if (bLeft)
					{
						Opt.CurBorder-=1;
					}
					else
					{
						Opt.CurBorder+=1;
					}
					Ret=true;
				}
				if (Ret)
				{
					Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
					Ret=UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo);
					Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
					if (Ret) return true;
					else return false;
				}
			}
			//----
			else if ( Param1==DlgLIST &&
								(Param2==KEY_LEFT || Param2==KEY_RIGHT ||
								Param2==(KEY_ALT|KEY_LEFT) || Param2==(KEY_RALT|KEY_LEFT) ||
								Param2==(KEY_ALT|KEY_RIGHT) || Param2==(KEY_RALT|KEY_RIGHT)) )
			{
				bool Ret=false;
				if (Param2==KEY_LEFT || Param2==KEY_RIGHT)
				{
					int maxDestCol=Opt.lenDestFileName-((DlgSize.Full?DlgSize.mW2:DlgSize.W2)-2)+Opt.CurBorder;
					if (Opt.destCurCol>maxDestCol)
					{
						if (maxDestCol>0) Opt.destCurCol=maxDestCol;
						else Opt.destCurCol=0;
					}
					if (Param2==KEY_LEFT && Opt.destCurCol>0)
					{
						Opt.destCurCol-=1; Ret=true;
					}
					else if (Param2==KEY_RIGHT && Opt.destCurCol<maxDestCol)
					{
						Opt.destCurCol+=1; Ret=true;
					}
				}
				else
				{
					int maxSrcCol=Opt.lenFileName-((DlgSize.Full?DlgSize.mW2:DlgSize.W2)-2)-Opt.CurBorder;
					if (Opt.srcCurCol>maxSrcCol)
					{
						if (maxSrcCol>0) Opt.srcCurCol=maxSrcCol;
						else Opt.srcCurCol=0;
					}
					if ((Param2==(KEY_ALT|KEY_LEFT) || Param2==(KEY_RALT|KEY_LEFT)) && Opt.srcCurCol>0)
					{
						Opt.srcCurCol-=1; Ret=true;
					}
					else if ((Param2==(KEY_ALT|KEY_RIGHT) || Param2==(KEY_RALT|KEY_RIGHT)) && Opt.srcCurCol<maxSrcCol)
					{
						Opt.srcCurCol+=1; Ret=true;
					}
				}
				if (Ret)
				{
					Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
					Ret=UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo);
					Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
				}
				return true;
			}
			//----
			else if ( Param1==DlgLIST &&
								(Param2==(KEY_CTRL|KEY_PGUP) || Param2==(KEY_RCTRL|KEY_PGUP)) )
			{
 GOTOFILE:
				int Pos=Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, 0);
				if (Pos>=(Opt.LoadUndo?Undo.iCount:FileList.Count())) 
					return true;
				string name;
				size_t size=Info.Control(PANEL_ACTIVE,FCTL_GETPANELDIR,0,0);
				wchar_t *buf=name.get(size);
				Info.Control(PANEL_ACTIVE,FCTL_GETPANELDIR,size,(LONG_PTR)buf);
				name.updsize();

				if (Opt.LoadUndo && FSF.LStricmp(name.get(),Undo.Dir))
					Info.Control(PANEL_ACTIVE,FCTL_SETPANELDIR,0,(LONG_PTR)Undo.Dir);

				PanelInfo PInfo;
				Info.Control(PANEL_ACTIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&PInfo);
				PanelRedrawInfo RInfo;
				RInfo.CurrentItem=PInfo.CurrentItem;
				RInfo.TopPanelItem=PInfo.TopPanelItem;

				name.clear();
				if (Opt.LoadUndo)
					name=Undo.CurFileName[Pos];
				else
				{
					File *cur=NULL; unsigned index;
					for (cur=FileList.First(), index=0; cur && index<Pos; cur=FileList.Next(cur), index++)
						;
					if (cur) name=cur->strSrcFileName;
				}

				for (int i=0; i<PInfo.ItemsNumber; i++)
				{
					PluginPanelItem *PPI=(PluginPanelItem*)malloc(Info.Control(PANEL_ACTIVE,FCTL_GETPANELITEM,i,0));
					if (PPI)
					{
						Info.Control(PANEL_ACTIVE,FCTL_GETPANELITEM,i,(LONG_PTR)PPI);
						if (!FSF.LStricmp(name.get(), PPI->FindData.lpwszFileName))
						{
							RInfo.CurrentItem=i;
							free(PPI);
							break;
						}
						else
							free(PPI);
					}
				}
				Info.Control(PANEL_ACTIVE,FCTL_REDRAWPANEL,0,(LONG_PTR)&RInfo);
				Info.SendDlgMessage(hDlg,DM_CLOSE,DlgCANCEL,0);
				return true;
			}
			break;

	/************************************************************************/

		case DN_EDITCHANGE:
			if (Param1==DlgEMASKNAME || Param1==DlgEMASKEXT ||
					Param1==DlgESEARCH || Param1==DlgEREPLACE )
			{
				if (Param1==DlgEMASKNAME)
					StrOpt.MaskName=((FarDialogItem *)Param2)->PtrData;
				else if (Param1==DlgEMASKEXT)
					StrOpt.MaskExt=((FarDialogItem *)Param2)->PtrData;
				else if (Param1==DlgESEARCH)
					StrOpt.Search=((FarDialogItem *)Param2)->PtrData;
				else if (Param1==DlgEREPLACE)
					StrOpt.Replace=((FarDialogItem *)Param2)->PtrData;

				if (ProcessFileName())
				{
					bError=false;
					Opt.destCurCol=0;
				}
				else bError=true;
				Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
				bool Ret=UpdateFarList(hDlg, DlgSize.Full, false);
				Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
				if (Ret) return true;
				else return false;
			}
			break;

	/************************************************************************/

		case DN_CTLCOLORDLGITEM:
			// !!! ��. ����������� � DN_GOTFOCUS.
			// ������ ��������� ��� ����������...
			if (Param1==DlgSEP2)
			{
				int color=Info.AdvControl(Info.ModuleNumber, ACTL_GETCOLOR, (void *)COL_DIALOGBOX);
				// ... ���� �������� � ������, �� ������ � ���������� ����
				if (DlgLIST==Info.SendDlgMessage(hDlg, DM_GETFOCUS, 0, 0))
				{
					Param2=Info.AdvControl(Info.ModuleNumber, ACTL_GETCOLOR, (void *)COL_DIALOGHIGHLIGHTBOXTITLE);
					Param2|=(color<<16);
				}
				// ... ����� - � ������� ����
				else
				{
					Param2=color;
					Param2|=(color<<16);
				}
				return Param2;
			}
			break;

	/************************************************************************/

		case DN_CLOSE:
			if (Param1==DlgREN && Opt.LoadUndo)
			{
				const wchar_t *MsgItems[]={ GetMsg(MUndoTitle), GetMsg(MUndoBody) };
				switch (Info.Message(Info.ModuleNumber,FMSG_WARNING|FMSG_MB_YESNOCANCEL,0,MsgItems,2,0))
				{
					case 0:
						Opt.Undo=1; return true;
					case 1:
						UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo=Opt.Undo=0);
						Info.SendDlgMessage(hDlg, DM_REDRAW, 0, 0);
						return false;
					default: return false;
				}
			}
			break;
	}

	return Info.DefDlgProc(hDlg, Msg, Param1, Param2);
}


/****************************************************************************
 * ������ ��������� �� �������, ���������� ������ � ������� ��������������,
 * ��������� ��������� Opt, ��������� (���� ����) ����� ��������� � �������.
 ****************************************************************************/
int VisRenDlg::ShowDialog()
{
	GetDlgSize();

	struct FarDialogItem DialogItems[] = {
		//			Type	X1	Y1	X2	Y2	Focus	Selected/History	Flags	DefaultButton	PtrData	MaxLen
		/* 0*/{DI_DOUBLEBOX,0,          0,DlgSize.W-1,DlgSize.H-1, 0, 0,                                0, 0, GetMsg(MVRenTitle), 0},
		/* 1*/{DI_TEXT,     DlgSize.W-4,0,DlgSize.W-2,          0, 0, 0,                                0, 0,                L"", 0},

		/* 2*/{DI_TEXT,     2,          1,          0,          0, 0, 0,                                0, 0,  GetMsg(MMaskName), 0},
		/* 3*/{DI_EDIT,     2,          2,DlgSize.WS-3,         0, 1, (DWORD_PTR)L"VisRenMaskName", DIF_USELASTHISTORY|DIF_HISTORY, 0,  L"", 0},
		/* 4*/{DI_TEXT,     2,          3,          0,          0, 0, 0,                                0, 0,     GetMsg(MTempl), 0},
		/* 5*/{DI_COMBOBOX, 2,          4,         31,          0, 0, 0,DIF_LISTAUTOHIGHLIGHT|DIF_DROPDOWNLIST|DIF_LISTWRAPMODE, 0, GetMsg(MTempl_1), 0},
		/* 6*/{DI_BUTTON,  34,          4,          0,          0, 0, 0,    DIF_NOBRACKETS|DIF_BTNNOCLOSE, 0,       GetMsg(MSet), 0},

		/* 7*/{DI_TEXT,     DlgSize.WS, 1,          0,          0, 0, 0,                                0, 0,   GetMsg(MMaskExt), 0},
		/* 8*/{DI_EDIT,     DlgSize.WS, 2,DlgSize.W-3,          0, 0, (DWORD_PTR)L"VisRenMaskExt",  DIF_USELASTHISTORY|DIF_HISTORY, 0,  L"", 0},
		/* 9*/{DI_COMBOBOX, DlgSize.WS, 4,DlgSize.W-8,          0, 0, 0,DIF_LISTAUTOHIGHLIGHT|DIF_DROPDOWNLIST|DIF_LISTWRAPMODE, 0, GetMsg(MTempl2_1), 0},
		/*10*/{DI_BUTTON,   DlgSize.W-5,4,          0,          0, 0, 0,    DIF_NOBRACKETS|DIF_BTNNOCLOSE, 0,       GetMsg(MSet), 0},


		/*11*/{DI_TEXT,     0,          5,          0,          0, 0, 0,                    DIF_SEPARATOR, 0,                L"", 0},
		/*12*/{DI_TEXT,     2,          6,         14,          0, 0, 0,                                0, 0,    GetMsg(MSearch), 0},
		/*13*/{DI_EDIT,    15,          6,DlgSize.WS-3,         0, 0, (DWORD_PTR)L"VisRenSearch",        DIF_HISTORY, 0,                L"", 0},
		/*14*/{DI_TEXT,     2,          7,         14,          0, 0, 0,                                0, 0,   GetMsg(MReplace), 0},
		/*15*/{DI_EDIT,    15,          7,DlgSize.WS-3,         0, 0, (DWORD_PTR)L"VisRenReplace",       DIF_HISTORY, 0,                L"", 0},
		/*16*/{DI_CHECKBOX, DlgSize.WS, 6,         19,          0, 0, 1,                                0, 0,      GetMsg(MCase), 0},
		/*17*/{DI_CHECKBOX, DlgSize.WS, 7,         19,          0, 0, 0,                                0, 0,     GetMsg(MRegEx), 0},

		/*18*/{DI_TEXT,     0,          8,          0,          0, 0, 0,                    DIF_SEPARATOR, 0,                L"", 0},
		/*19*/{DI_LISTBOX,  2,          9,DlgSize.W-2,DlgSize.H-4, 0, 0,    DIF_LISTNOCLOSE|DIF_LISTNOBOX, 0,                L"", 0},
		/*20*/{DI_TEXT,     0,DlgSize.H-3,          0,          0, 0, 0,                    DIF_SEPARATOR, 0,                L"", 0},

		/*21*/{DI_BUTTON,   0,DlgSize.H-2,          0,          0, 0, 0,                  DIF_CENTERGROUP, 1,       GetMsg(MRen), 0},
		/*22*/{DI_BUTTON,   0,DlgSize.H-2,          0,          0, 0, 0,   DIF_BTNNOCLOSE|DIF_CENTERGROUP, 0,      GetMsg(MUndo), 0},
		/*23*/{DI_BUTTON,   0,DlgSize.H-2,          0,          0, 0, 0,                  DIF_CENTERGROUP, 0,      GetMsg(MEdit), 0},
		/*24*/{DI_BUTTON,   0,DlgSize.H-2,          0,          0, 0, 0,                  DIF_CENTERGROUP, 0,    GetMsg(MCancel), 0}
	};

	// ��������������� ������ � ���������
	FarListItem itemTempl1[26];
	int n = sizeof(itemTempl1) / sizeof(itemTempl1[0]);
	for (int i = 0; i < n; i++)
	{
		itemTempl1[i].Flags=((i==3 || i==9 || i==16 || i==21)?LIF_SEPARATOR:0);
		itemTempl1[i].Text=GetMsg(MTempl_1+i);
		itemTempl1[i].Reserved[0]=itemTempl1[i].Reserved[1]=itemTempl1[i].Reserved[2]=0;
	}
	FarList Templates1 = {n, itemTempl1};
	DialogItems[DlgETEMPLNAME].ListItems = &Templates1;

	FarListItem itemTempl2[8];
	n = sizeof(itemTempl2) / sizeof(itemTempl2[0]);
	for (int i = 0; i < n; i++)
	{
		itemTempl2[i].Flags=(i==3?LIF_SEPARATOR:0);
		itemTempl2[i].Text=GetMsg(MTempl2_1+i);
		itemTempl2[i].Reserved[0]=itemTempl2[i].Reserved[1]=itemTempl2[i].Reserved[2]=0;
	}
	FarList Templates2 = {n, itemTempl2};
	DialogItems[DlgETEMPLEXT].ListItems = &Templates2;

	HANDLE hDlg=Info.DialogInit( Info.ModuleNumber,
                               -1, -1, DlgSize.W, DlgSize.H,
                               L"Contents",
                               DialogItems,
                               sizeof(DialogItems) / sizeof(DialogItems[0]),
                               0, FDLG_SMALLDIALOG,
                               ShowDialogProcThunk,
                               (LONG_PTR) this );

	int ExitCode=3;

	if (hDlg != INVALID_HANDLE_VALUE)
	{
		ExitCode=Info.DialogRun(hDlg);
		switch (ExitCode)
		{
			case DlgREN:
				Opt.UseLastHistory=1;
				ExitCode=0;
				break;
			case DlgEDIT:
				ExitCode=2;
				break;
			default:
				ExitCode=3;
				break;
		}
		Info.DialogFree(hDlg);
	}
	return ExitCode;
}