#include <windows.h>
#include <windowsx.h>
#include <process.h>
#include <tchar.h>
#include <stdio.h>
#include "CmnHdr.h"
#include "Resource.h"
#include "Job.h"

CJob g_job;
HWND g_hwnd;
HANDLE g_hIOCP;
HANDLE g_hThreadIOCP;

#define COMPKEY_TERMINATE ((UINT_PTR) 0)
#define COMPKEY_STATUS ((UINT_PTR) 1)
#define COMPKEY_JOBOBJECT ((UINT_PTR) 2)

DWORD WINAPI JobNotify(PVOID) {
	TCHAR sz[2000];
	BOOL fDone = FALSE;

	while (!fDone) {
		DWORD dwBytesXferred;
		ULONG_PTR CompKey;
		LPOVERLAPPED po;
		GetQueuedCompletionStatus(g_hIOCP, &dwBytesXferred, &CompKey, &po, INFINITE);

		fDone = (CompKey == COMPKEY_TERMINATE);

		HWND hwndLB = FindWindow(NULL, TEXT("Job Lab"));
		hwndLB = GetDlgItem(hwndLB, IDC_STATUS);

		if (CompKey == COMPKEY_JOBOBJECT) {
			lstrcpy(sz, TEXT("--> Notification: "));
			PTSTR psz = sz + lstrlen(sz);
			switch (dwBytesXferred) {
			case JOB_OBJECT_MSG_END_OF_JOB_TIME:
				wsprintf(psz, TEXT("Job time limit reached"));
				break;

			case JOB_OBJECT_MSG_END_OF_PROCESS_TIME:
				wsprintf(psz, TEXT("Job process (Id=%d) time limit reached"), po);
				break;

			case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT:
				wsprintf(psz, TEXT("Too many active processes in job"));
				break;

			case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO:
				wsprintf(psz, TEXT("Job contains no active processes"));
				break;

			case JOB_OBJECT_MSG_NEW_PROCESS:
				wsprintf(psz, TEXT("New process (Id=%d) in Job"), po);
				break;

			case JOB_OBJECT_MSG_EXIT_PROCESS:
				wsprintf(psz, TEXT("Process (Id=%d) terminated"), po);
				break;

			case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:
				wsprintf(psz, TEXT("Process (Id=%d) terminated abnormally"), po);
				break;

			case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT:
				wsprintf(psz, TEXT("Process (Id=%d) exceeded memory limit"), po);
				break;

			case JOB_OBJECT_MSG_JOB_MEMORY_LIMIT:
				wsprintf(psz, TEXT("Process (Id=%d) exceeded job memory limit"), po);
				break;

			default:
				wsprintf(psz, TEXT("Unknown notification: %d"), dwBytesXferred);
				break;
			}
			ListBox_SetCurSel(hwndLB, ListBox_AddString(hwndLB, sz));
			CompKey = 1;
		}

		if (CompKey == COMPKEY_STATUS) {
			static int s_nStatusCount = 0;
			_stprintf_s(sz, TEXT("--> Status Update (%u)"), s_nStatusCount++);
			ListBox_SetCurSel(hwndLB, ListBox_AddString(hwndLB, sz));

			JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION jobai;
			g_job.QueryBasicAccountingInfo(&jobai);

			_stprintf_s(sz, TEXT("Total Time: User=%I64u, Kernel=%I64u        ")
				TEXT("Period Time: User=%I64u, Kernel=%I64u"),
				jobai.BasicInfo.TotalUserTime.QuadPart,
				jobai.BasicInfo.TotalKernelTime.QuadPart,
				jobai.BasicInfo.ThisPeriodTotalUserTime.QuadPart,
				jobai.BasicInfo.ThisPeriodTotalKernelTime.QuadPart);
			ListBox_SetCurSel(hwndLB, ListBox_AddString(hwndLB, sz));

			_stprintf_s(sz, TEXT("Page Faults=%u, Total Processes=%u, ")
				TEXT("Active Processes=%u, Terminated Processes=%u"),
				jobai.BasicInfo.TotalPageFaultCount,
				jobai.BasicInfo.TotalProcesses,
				jobai.BasicInfo.ActiveProcesses,
				jobai.BasicInfo.TotalTerminatedProcesses);
			ListBox_SetCurSel(hwndLB, ListBox_AddString(hwndLB, sz));
			
			_stprintf_s(sz, TEXT("Reads=%I64u (%I64u Bytes), ")
				TEXT("Write=%I64u (%I64u bytes), Other=%I64u (%I64u bytes)"),
				jobai.IoInfo.ReadOperationCount, jobai.IoInfo.ReadTransferCount,
				jobai.IoInfo.WriteOperationCount, jobai.IoInfo.WriteTransferCount,
				jobai.IoInfo.OtherOperationCount, jobai.IoInfo.OtherTransferCount);
			ListBox_SetCurSel(hwndLB, ListBox_AddString(hwndLB, sz));

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION joeli;
			g_job.QueryExtendedLimitInfo(&joeli);
			_stprintf_s(sz, TEXT("Peak memory used: Process=%I64u, Job=%I64u"),
				(__int64)joeli.PeakProcessMemoryUsed,
				(__int64)joeli.PeakJobMemoryUsed);
			ListBox_SetCurSel(hwndLB, ListBox_AddString(hwndLB, sz));

			DWORD dwNumProcesses = 50, dwProcessIdList[50];
			g_job.QueryBasicProcessIdList(dwNumProcesses, dwProcessIdList, &dwNumProcesses);
			_stprintf_s(sz, TEXT("PIDs: %s"), (dwNumProcesses == 0) ? TEXT("(none)") : TEXT(""));
			for (DWORD x = 0; x < dwNumProcesses; x++) {
				_stprintf_s(sz, TEXT("%d "), dwProcessIdList[x]);
			}
			ListBox_SetCurSel(hwndLB, ListBox_AddString(hwndLB, sz));
		}
	}
	return 0;
}

BOOL Dlg_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam) {
	chSETDLGICONS(hwnd, IDI_JOBLAB);
	g_hwnd = hwnd;

	HWND hwndPriorityClass = GetDlgItem(hwnd, IDC_PRIORITYCLASS);
	ComboBox_AddString(hwndPriorityClass, TEXT("No limit"));
	ComboBox_AddString(hwndPriorityClass, TEXT("Idle"));
	ComboBox_AddString(hwndPriorityClass, TEXT("Below normal"));
	ComboBox_AddString(hwndPriorityClass, TEXT("Normal"));
	ComboBox_AddString(hwndPriorityClass, TEXT("Above normal"));
	ComboBox_AddString(hwndPriorityClass, TEXT("High"));
	ComboBox_AddString(hwndPriorityClass, TEXT("Realtime"));
	ComboBox_SetCurSel(hwndPriorityClass, 0);

	HWND hwndSchedulingClass = GetDlgItem(hwnd, IDC_SCHEDULINGCLASS);
	ComboBox_AddString(hwndSchedulingClass, TEXT("No limit"));
	for (int n = 0; n <= 9; n++) {
		TCHAR szSchedulingClass[2] = { (TCHAR)(TEXT('0') + n), 0 };
		ComboBox_AddString(hwndSchedulingClass, szSchedulingClass);
	}
	ComboBox_SetCurSel(hwndSchedulingClass, 0);
	SetTimer(hwnd, 1, 10000, NULL);
	return TRUE;
}

void Dlg_ApplyLimits(HWND hwnd) {
	const int nNanosecondsPerSecond = 100000000;
	const int nMillisecondsPerSecond = 1000;
	const int nNanosecondsPerMillisecond = nNanosecondsPerSecond / nMillisecondsPerSecond;
	BOOL f;
	__int64 q;
	SIZE_T s;
	DWORD d;

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION joeli = { 0 };
	joeli.BasicLimitInformation.LimitFlags = 0;

	q = GetDlgItemInt(hwnd, IDC_PERPROCESSUSERTIMELIMIT, &f, FALSE);
	if (f) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
		joeli.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = 1 * nNanosecondsPerMillisecond / 100;
	}

	q = GetDlgItemInt(hwnd, IDC_MINWORKINGSETSIZE, &f, FALSE);
	if (f) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_TIME;
		joeli.BasicLimitInformation.PerJobUserTimeLimit.QuadPart = q * nNanosecondsPerMillisecond / 100;
	}

	s = GetDlgItemInt(hwnd, IDC_MINWORKINGSETSIZE, &f, FALSE);
	if (f) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_WORKINGSET;
		joeli.BasicLimitInformation.MinimumWorkingSetSize = s * 1024 * 1024;
		s = GetDlgItemInt(hwnd, IDC_MAXWORKINGSETSIZE, &f, FALSE);
		if (f) {
			joeli.BasicLimitInformation.MaximumWorkingSetSize = s * 1024 * 1024;
		}
		else {
			joeli.BasicLimitInformation.LimitFlags &= ~JOB_OBJECT_LIMIT_WORKINGSET;
			chMB("Both minimum and maximum working set sizes must be set.\n"
				"The working set limits will NOT be in effect.");
		}
	}

	d = GetDlgItemInt(hwnd, IDC_ACTIVEPROCESSLIMIT, &f, FALSE);
	if (f) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
		joeli.BasicLimitInformation.ActiveProcessLimit = d;
	}

	s = GetDlgItemInt(hwnd, IDC_AFFINITYMASK, &f, FALSE);
	if (f) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_AFFINITY;
		joeli.BasicLimitInformation.Affinity = s;
	}

	joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PRIORITY_CLASS;
	switch (ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_PRIORITYCLASS))) {
	case 0:
		joeli.BasicLimitInformation.LimitFlags &= ~JOB_OBJECT_LIMIT_PRIORITY_CLASS;
		break;

	case 1:
		joeli.BasicLimitInformation.PriorityClass = IDLE_PRIORITY_CLASS;
		break;

	case 2:
		joeli.BasicLimitInformation.PriorityClass = BELOW_NORMAL_PRIORITY_CLASS;
		break;

	case 3:
		joeli.BasicLimitInformation.PriorityClass = NORMAL_PRIORITY_CLASS;
		break;

	case 4:
		joeli.BasicLimitInformation.PriorityClass = ABOVE_NORMAL_PRIORITY_CLASS;
		break;

	case 5:
		joeli.BasicLimitInformation.PriorityClass = HIGH_PRIORITY_CLASS;
		break;

	case 6:
		joeli.BasicLimitInformation.PriorityClass = REALTIME_PRIORITY_CLASS;
		break;
	}

	int nSchedulingClass = ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_SCHEDULINGCLASS));
	if (nSchedulingClass > 0) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_SCHEDULING_CLASS;
		joeli.BasicLimitInformation.SchedulingClass = nSchedulingClass - 1;
	}

	s = GetDlgItemInt(hwnd, IDC_MAXCOMMITPERJOB, &f, FALSE);
	if (f) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
		joeli.JobMemoryLimit = s * 1024 * 1024;
	}

	s = GetDlgItemInt(hwnd, IDC_MAXCOMMITPERPROCESS, &f, FALSE);
	if (f) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
		joeli.ProcessMemoryLimit = s * 1024 * 1024;
	}

	if (IsDlgButtonChecked(hwnd, IDC_CHILDPROCESSESCANBREAKAWAYFROMJOB)) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_BREAKAWAY_OK;
	}

	if (IsDlgButtonChecked(hwnd, IDC_CHILDPROCESSESDOBREAKAWAYFROMJOB)) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
	}

	if (IsDlgButtonChecked(hwnd, IDC_TERMINATEPROCESSONEXCEPTIONS)) {
		joeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
	}

	f = g_job.SetExtendedLimitInfo(&joeli, ((joeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_JOB_TIME) != 0)
		? FALSE
		: IsDlgButtonChecked(hwnd, IDC_PRESERVEJOBTIMEWHENAPPLYINGLIMITS));
	chASSERT(f);
	
	DWORD jobuir = JOB_OBJECT_UILIMIT_NONE;
	if (IsDlgButtonChecked(hwnd, IDC_RESTRICTACCESSTOOUTSIDEUSEROBJECTS)) {
		jobuir |= JOB_OBJECT_UILIMIT_HANDLES;
	}

	if (IsDlgButtonChecked(hwnd, IDC_RESTRICTREADINGCLIPBOARD)) {
		jobuir |= JOB_OBJECT_UILIMIT_READCLIPBOARD;
	}

	if (IsDlgButtonChecked(hwnd, IDC_RESTRICTWRITINGCLIPBOARD)) {
		jobuir |= JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
	}

	if (IsDlgButtonChecked(hwnd, IDC_RESTRICTEXITWINDOW)) {
		jobuir |= JOB_OBJECT_UILIMIT_EXITWINDOWS;
	}

	if (IsDlgButtonChecked(hwnd, IDC_RESTRICTCHANGINGSYSTEMPARAMETERS)) {
		jobuir |= JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS;
	}

	if (IsDlgButtonChecked(hwnd, IDC_RESTRICTDESKTOPS)) {
		jobuir |= JOB_OBJECT_UILIMIT_DESKTOP;
	}

	if (IsDlgButtonChecked(hwnd, IDC_RESTRICTDISPLAYSETTINGS)) {
		jobuir |= JOB_OBJECT_UILIMIT_DISPLAYSETTINGS;
	}

	if (IsDlgButtonChecked(hwnd, IDC_RESTRICTGLOBALATOMS)) {
		jobuir |= JOB_OBJECT_UILIMIT_GLOBALATOMS;
	}

	chVERIFY(g_job.SetBasicUIRestrictions(jobuir));
}

void Dlg_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
	switch (id) {
	case IDCANCEL:
		KillTimer(hwnd, 1);
		g_job.Terminate(0);
		EndDialog(hwnd, id);
		break;

	case IDC_PERJOBUSERTIMELIMIT:
	{
		BOOL f;
		GetDlgItemInt(hwnd, IDC_PERJOBUSERTIMELIMIT, &f, FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_PRESERVEJOBTIMEWHENAPPLYINGLIMITS), !f);
	}
	break;

	case IDC_APPLYLIMITS:
		Dlg_ApplyLimits(hwnd);
		PostQueuedCompletionStatus(g_hIOCP, 0, COMPKEY_STATUS, NULL);
		break;

	case IDC_TERMINATE:
		g_job.Terminate(0);
		PostQueuedCompletionStatus(g_hIOCP, 0, COMPKEY_STATUS, NULL);
		break;

	case IDC_SPAWNCMDINJOB:
	{
		STARTUPINFO si = { sizeof(si) };
		PROCESS_INFORMATION pi;
		TCHAR sz[] = TEXT("CMD");
		CreateProcess(NULL, sz, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
		g_job.AssignProcess(pi.hProcess);
		ResumeThread(pi.hThread);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	PostQueuedCompletionStatus(g_hIOCP, 0, COMPKEY_STATUS, NULL);
	break;

	case IDC_ASSIGNPROCESSTOJOB:
	{
		DWORD dwProcessId = GetDlgItemInt(hwnd, IDC_PROCESSID, NULL, FALSE);
		HANDLE hProcess = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, dwProcessId);
		if (hProcess != NULL) {

			chVERIFY(g_job.AssignProcess(hProcess));
			CloseHandle(hProcess);
		}
		else chMB("Could not assign Process to job.");
	}
	PostQueuedCompletionStatus(g_hIOCP, 0, COMPKEY_STATUS, NULL);
	break;
	}
}

void WINAPI Dlg_OnTimer(HWND hwnd, UINT id) {
	PostQueuedCompletionStatus(g_hIOCP, 0, COMPKEY_STATUS, NULL);
}

INT_PTR WINAPI Dlg_Proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		chHANDLE_DLGMSG(hwnd, WM_INITDIALOG, Dlg_OnInitDialog);
		chHANDLE_DLGMSG(hwnd, WM_TIMER, Dlg_OnTimer);
		chHANDLE_DLGMSG(hwnd, WM_COMMAND, Dlg_OnCommand);
	}
	return FALSE;
}

int WINAPI _tWinMain(HINSTANCE hinstExe, HINSTANCE, PTSTR pszCmdLine, int) {
	g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	g_hThreadIOCP = chBEGINTHREADEX(NULL, 0, JobNotify, NULL, 0, NULL);
	g_job.Create(NULL, TEXT("JobLab"));
	g_job.SetEndOfJobInfo(JOB_OBJECT_POST_AT_END_OF_JOB);
	g_job.AssociateCompletionPort(g_hIOCP, COMPKEY_JOBOBJECT);

	DialogBox(hinstExe, MAKEINTRESOURCE(IDD_JOBLAB), NULL, Dlg_Proc);

	PostQueuedCompletionStatus(g_hIOCP, 0, COMPKEY_TERMINATE, NULL);

	WaitForSingleObject(g_hThreadIOCP, INFINITE);

	CloseHandle(g_hIOCP);
	CloseHandle(g_hThreadIOCP);

	return 0;
}