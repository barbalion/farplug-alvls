enum {
t_week=6,
t_month=30,
t_quarter=91,
t_halfyear=182,
t_year=365,
t_period=0
};

void SetPeriod(HANDLE hDlg,int Period)
{
	wchar_t Buf[11];
	SYSTEMTIME st;
	GetLocalTime(&st);
	if (ipc.opt.Date)
		FSF.sprintf(Buf,L"%04d-%02d-%02d",st.wYear,st.wMonth,st.wDay);
	else
		FSF.sprintf(Buf,L"%02d-%02d-%04d",st.wDay,st.wMonth,st.wYear);
	Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,4,Buf);
	Info.SendDlgMessage(hDlg,DM_EDITUNCHANGEDFLAG,4,(void*)1);

	FILETIME ft;
	SystemTimeToFileTime(&st,&ft);
	ULARGE_INTEGER ut_in, ut_out;
	ut_in.LowPart=ft.dwLowDateTime;
	ut_in.HighPart=ft.dwHighDateTime;
	ut_out.QuadPart=ut_in.QuadPart-(ULONGLONG(10000000)*60*60*24*Period);
	ft.dwLowDateTime=ut_out.LowPart;
	ft.dwHighDateTime=ut_out.HighPart;
	FileTimeToSystemTime(&ft,&st);
	if (ipc.opt.Date)
		FSF.sprintf(Buf,L"%04d-%02d-%02d",st.wYear,st.wMonth,st.wDay);
	else
		FSF.sprintf(Buf,L"%02d-%02d-%04d",st.wDay,st.wMonth,st.wYear);
	Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,2,Buf);
	Info.SendDlgMessage(hDlg,DM_EDITUNCHANGEDFLAG,2,(void*)1);
}

bool CheckDate(wchar_t *StrData)
{
	int num=StringToNumber(StrData,num);
	if (num<1900)
		return false;
	num=StringToNumber(StrData+5,num);
	if (num<1||num>12)
		return false;
	num=StringToNumber(StrData+8,num);
	if (num<1||num>31)
		return false;
	return true;
}

intptr_t WINAPI GetNewModulesDialogProc(HANDLE hDlg,intptr_t Msg,intptr_t Param1,void *Param2)
{
	switch(Msg)
	{
		case DN_INITDIALOG:
			SetPeriod(hDlg,t_week);
			return true;

	/************************************************************************/

		case DN_LISTHOTKEY:  // ��� ������� ���
		case DN_LISTCHANGE:  //  ���� � ����
			if (Param1==1)
			{
				switch((intptr_t)Param2)
				{
					case 0:
						SetPeriod(hDlg,t_week);
						break;
					case 1:
						SetPeriod(hDlg,t_month);
						break;
					case 2:
						SetPeriod(hDlg,t_quarter);
						break;
					case 3:
						SetPeriod(hDlg,t_halfyear);
						break;
					case 4:
						SetPeriod(hDlg,t_year);
						break;
					case 5:
						SetPeriod(hDlg,t_period);
						break;
				}
				return Msg==DN_LISTHOTKEY?false:true;    // ��� ������� ������� �������� - ������� ������
			}

	/************************************************************************/

		case DN_CLOSE:
			if (Param1==6)
			{
				if (ipc.opt.Date)
				{
					lstrcpy(ipc.opt.DateFrom,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,2,0));
					lstrcpy(ipc.opt.DateTo,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,4,0));
				}
				else
				{
					CopyReverseTime2(ipc.opt.DateFrom,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,2,0));
					CopyReverseTime2(ipc.opt.DateTo,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,4,0));
				}
				if (CheckDate(ipc.opt.DateFrom) && CheckDate(ipc.opt.DateTo) && FSF.LStricmp(ipc.opt.DateFrom,ipc.opt.DateTo)<=0)
					return true;
				else
				{
					MessageBeep(MB_ICONASTERISK);
					return false;
				}
			}

	}
	return Info.DefDlgProc(hDlg,Msg,Param1,Param2);
}


bool GetNewModulesDialog()
{
	const wchar_t *mask=ipc.opt.Date?L"9999-99-99":L"99-99-9999";
	struct FarDialogItem DialogItems[] = {
		//			Type	X1	Y1	X2	Y2	Selected	History	Mask	Flags	Data	MaxLen	UserParam
		/* 0*/{DI_DOUBLEBOX,  3, 1,54, 5, 0, 0, 0,                           0, MSG(MNewModules),0,0},
		/* 1*/{DI_COMBOBOX,   5, 2,25, 0, 0, 0, 0, DIF_LISTAUTOHIGHLIGHT|DIF_DROPDOWNLIST|DIF_LISTWRAPMODE|DIF_FOCUS, MSG(MTime_0), 0,0},
		/* 2*/{DI_FIXEDIT,   30, 2,39, 0, 0, 0, mask,                          DIF_MASKEDIT,L"",0,0},
		/* 3*/{DI_TEXT,      41, 2,41, 0, 0, 0, 0,                                       0, L"-",0,0},
		/* 4*/{DI_FIXEDIT,   43, 2,52, 0, 0, 0, mask,                          DIF_MASKEDIT,L"",0,0},

		/* 5*/{DI_TEXT,      -1, 3, 0, 0, 0, 0, 0,                            DIF_SEPARATOR, L"",0,0},
		/* 6*/{DI_BUTTON,     0, 4, 0, 0, 0, 0, 0,   DIF_DEFAULTBUTTON|DIF_CENTERGROUP, MSG(MOK),0,0},
		/* 7*/{DI_BUTTON,     0, 4, 0, 0, 0, 0, 0,                 DIF_CENTERGROUP, MSG(MCancel),0,0}
	};

	// ��������������� ������ � ���������
	FarListItem itemTime[6];
	int n = sizeof(itemTime) / sizeof(itemTime[0]);
	for (int i = 0; i < n; i++)
	{
		itemTime[i].Flags=0;
		itemTime[i].Text=MSG(MTime_0+i);
		itemTime[i].Reserved[0]=itemTime[i].Reserved[1]=0;
	}
	FarList Time = {sizeof(FarList), n, itemTime};
	DialogItems[1].ListItems = &Time;

	HANDLE hDlg=Info.DialogInit(&MainGuid, &GetNewModulesDlgGuid,-1,-1,58,7,L"Contents",DialogItems,ARRAYSIZE(DialogItems),0,0,GetNewModulesDialogProc,0);

	bool ret=false;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		if (Info.DialogRun(hDlg)==6)
		{
			wchar_t Buf[80];
			FSF.sprintf(Buf,L"%s - %s - %s",(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,2,0),MSG(MNewModules),(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,4,0));
			Info.SendDlgMessage(::hDlg,DM_SETTEXTPTR,DlgBORDER,Buf);
			ret=true;
		}
		Info.DialogFree(hDlg);
	}
	return ret;
}
