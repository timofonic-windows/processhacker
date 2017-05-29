/*
 * Process Hacker Plugins -
 *   Update Checker Plugin
 *
 * Copyright (C) 2011-2016 dmex
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "updater.h"

static HWND LogDialogHandle = NULL;
static HANDLE LogDialogThreadHandle = NULL;
static PH_EVENT LogDialogInitialized = PH_EVENT_INIT;

INT_PTR CALLBACK OptionsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PhCenterWindow(hwndDlg, GetParent(hwndDlg));

            if (PhGetIntegerSetting(SETTING_NAME_AUTO_CHECK))
                Button_SetCheck(GetDlgItem(hwndDlg, IDC_AUTOCHECKBOX), BST_CHECKED);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDCANCEL:
                {
                    PhSetIntegerSetting(SETTING_NAME_AUTO_CHECK,
                        Button_GetCheck(GetDlgItem(hwndDlg, IDC_AUTOCHECKBOX)) == BST_CHECKED);

                    EndDialog(hwndDlg, IDCANCEL);
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

VOID ShowOptionsDialog(
    _In_opt_ HWND Parent
    )
{
    DialogBox(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_OPTIONS),
        Parent,
        OptionsDlgProc
        );
}

#include <Richedit.h>

PPH_BYTES BuildMessageToRtfString(
    _In_ PPH_STRING BuildMessage
    )
{
    static PH_STRINGREF skipPoolTagFileHeader = PH_STRINGREF_INIT(L" ");
    static PH_STRINGREF skipPoolTagFileLine = PH_STRINGREF_INIT(L"\n");
    PH_STRING_BUILDER rtfsb;
    PPH_STRING rtfString;
    PPH_BYTES rtf8String;
    PH_STRINGREF firstPart;
    PH_STRINGREF remainingPart;
    PH_STRINGREF poolTagPart;
    PH_STRINGREF binaryNamePart;
    PH_STRINGREF descriptionPart;

    remainingPart = BuildMessage->sr;

    PhInitializeStringBuilder(&rtfsb, 0x100);
    PhAppendStringBuilder2(&rtfsb,
        L"{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Calibri; }}"
        L"{\\colortbl;\\red0\\green77\\blue187;\\red0\\green0\blue0;}"
        L"\\landscape\\paperw15840\\paperh12240\\margl720\\margr720\\margt720\\margb720\\tx720\\tx1440\\tx2880\\tx5760");

    PhSplitStringRefAtString(&remainingPart, &skipPoolTagFileLine, TRUE, &firstPart, &remainingPart);

    while (remainingPart.Length != 0)
    {
        PPH_STRING dateTime;
        PPH_STRING authorName;
        PPH_STRING commitMessage;

        if (!PhSplitStringRefAtChar(&remainingPart, ' ', &poolTagPart, &remainingPart))
            continue;
        if (!PhSplitStringRefAtChar(&remainingPart, ' ', &binaryNamePart, &remainingPart))
            continue;

        PhSplitStringRefAtChar(&remainingPart, '\n', &descriptionPart, &remainingPart);

        dateTime = PhCreateString2(&poolTagPart);
        authorName = PhCreateString2(&binaryNamePart);
        commitMessage = PhCreateString2(&descriptionPart);

        PhAppendFormatStringBuilder(
            &rtfsb, 
            L"%s \\b\\cf1 %s \\cf0\\b0 %s \\line", 
            PhGetStringOrEmpty(dateTime), 
            PhGetStringOrEmpty(authorName),
            PhGetStringOrEmpty(commitMessage)
            );

        PhDereferenceObject(commitMessage);
        PhDereferenceObject(authorName);
        PhDereferenceObject(dateTime);
    }

    PhAppendStringBuilder2(&rtfsb, L"}");

    rtfString = PH_AUTO(PhFinalStringBuilderString(&rtfsb));
    rtf8String = PH_AUTO(PhConvertUtf16ToUtf8(rtfString->Buffer));

    return rtf8String;
}

INT_PTR CALLBACK TextDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER LayoutManager;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PPH_UPDATER_CONTEXT context = (PPH_UPDATER_CONTEXT)lParam;

            SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)PH_LOAD_SHARED_ICON_SMALL(PhLibImageBase, MAKEINTRESOURCE(PHAPP_IDI_PROCESSHACKER)));
            SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)PH_LOAD_SHARED_ICON_LARGE(PhLibImageBase, MAKEINTRESOURCE(PHAPP_IDI_PROCESSHACKER)));
           
            SendMessage(GetDlgItem(hwndDlg, IDC_TEXT), EM_AUTOURLDETECT, TRUE, 0);

            if (!PhIsNullOrEmptyString(context->BuildMessage))
            {
                SETTEXTEX s;

                s.flags = ST_UNICODE;
                s.codepage = CP_ACP;

                SendMessage(GetDlgItem(hwndDlg, IDC_TEXT), EM_SETTEXTMODE, (WPARAM)TM_RICHTEXT, 0);
                SendMessage(GetDlgItem(hwndDlg, IDC_TEXT), EM_SETTEXTEX, (WPARAM)&s, (LPARAM)BuildMessageToRtfString(context->BuildMessage)->Buffer);
            }

            //SendMessage(GetDlgItem(hwndDlg, IDC_TEXT), EM_SETEVENTMASK, 0, ENM_LINK);
            //SendMessage(GetDlgItem(hwndDlg, IDC_TEXT), EM_SETBKGNDCOLOR, 0, (LPARAM)GetSysColor(COLOR_BTNFACE));

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_TEXT), NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDCANCEL), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);

            if (PhGetIntegerPairSetting(SETTING_NAME_CHANGELOG_WINDOW_POSITION).X != 0)
                PhLoadWindowPlacementFromSetting(SETTING_NAME_CHANGELOG_WINDOW_POSITION, SETTING_NAME_CHANGELOG_WINDOW_SIZE, hwndDlg);
            else
                PhCenterWindow(hwndDlg, GetParent(hwndDlg));

            SendMessage(hwndDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwndDlg, IDCANCEL), TRUE);
        }
        break;
    case WM_DESTROY:
        {
            PhSaveWindowPlacementToSetting(SETTING_NAME_CHANGELOG_WINDOW_POSITION, SETTING_NAME_CHANGELOG_WINDOW_SIZE, hwndDlg);

            PhDeleteLayoutManager(&LayoutManager);

            PostQuitMessage(0);
        }
        break;
    case WM_SIZE:
        {
            PhLayoutManagerLayout(&LayoutManager);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDCANCEL:
                DestroyWindow(hwndDlg);
                break;
            }
        }
        break;
    }

    return FALSE;
}

NTSTATUS LogWindowDialogThread(
    _In_ PVOID Parameter
    )
{
    BOOL result;
    MSG message;
    PH_AUTO_POOL autoPool;

    LoadLibrary(L"msftedit.dll");

    PhInitializeAutoPool(&autoPool);

    LogDialogHandle = CreateDialogParam(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_TEXT),
        NULL,
        TextDlgProc,
        (LPARAM)Parameter
        );

    ShowWindow(LogDialogHandle, SW_SHOW);
    SetForegroundWindow(LogDialogHandle);

    PhSetEvent(&LogDialogInitialized);

    while (result = GetMessage(&message, NULL, 0, 0))
    {
        if (result == -1)
            break;

        if (!IsDialogMessage(LogDialogHandle, &message))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        PhDrainAutoPool(&autoPool);
    }

    PhDeleteAutoPool(&autoPool);

    if (LogDialogThreadHandle)
    {
        NtClose(LogDialogThreadHandle);
        LogDialogThreadHandle = NULL;
    }

    PhResetEvent(&LogDialogInitialized);

    return STATUS_SUCCESS;
}

VOID ShowChangelogDialog(
    _In_opt_ PPH_UPDATER_CONTEXT Context
    )
{
    if (!LogDialogThreadHandle)
    {
        if (!(LogDialogThreadHandle = PhCreateThread(0, LogWindowDialogThread, Context)))
        {
            PhShowStatus(Context->DialogHandle, L"Unable to create the changelog window.", 0, GetLastError());
            return;
        }

        PhWaitForEvent(&LogDialogInitialized, NULL);
    }

    PostMessage(LogDialogHandle, WM_INITDIALOG, 0, 0);
}