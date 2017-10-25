/*
 * Process Hacker -
 *   Main window
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017 dmex
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

#include <phapp.h>
#include <mainwnd.h>

#include <windowsx.h>
#include <winsta.h>

#include <cpysave.h>
#include <emenu.h>
#include <kphuser.h>
#include <svcsup.h>
#include <symprv.h>
#include <verify.h>
#include <workqueue.h>
#include <phsettings.h>

#include <actions.h>
#include <memsrch.h>
#include <miniinfo.h>
#include <netlist.h>
#include <netprv.h>
#include <notifico.h>
#include <phplug.h>
#include <phsvccl.h>
#include <procprp.h>
#include <procprv.h>
#include <proctree.h>
#include <settings.h>
#include <srvlist.h>
#include <srvprv.h>
#include <sysinfo.h>

#include <mainwndp.h>

#define RUNAS_MODE_ADMIN 1
#define RUNAS_MODE_LIMITED 2

PHAPPAPI HWND PhMainWndHandle;
BOOLEAN PhMainWndExiting = FALSE;
HMENU PhMainWndMenuHandle;

PH_PROVIDER_REGISTRATION PhMwpProcessProviderRegistration;
PH_PROVIDER_REGISTRATION PhMwpServiceProviderRegistration;
PH_PROVIDER_REGISTRATION PhMwpNetworkProviderRegistration;
BOOLEAN PhMwpUpdateAutomatically = TRUE;

ULONG PhMwpNotifyIconNotifyMask;
ULONG PhMwpLastNotificationType;
PH_MWP_NOTIFICATION_DETAILS PhMwpLastNotificationDetails;

static BOOLEAN NeedsMaximize = FALSE;
static BOOLEAN AlwaysOnTop = FALSE;

static BOOLEAN DelayedLoadCompleted = FALSE;

static PH_CALLBACK_DECLARE(LayoutPaddingCallback);
static RECT LayoutPadding = { 0, 0, 0, 0 };
static BOOLEAN LayoutPaddingValid = TRUE;

static HWND TabControlHandle;
static PPH_LIST PageList;
static PPH_MAIN_TAB_PAGE CurrentPage;
static INT OldTabIndex;
static HFONT CurrentCustomFont;

static HMENU SubMenuHandles[5];
static PPH_EMENU SubMenuObjects[5];

static PH_CALLBACK_REGISTRATION SymInitRegistration;

static ULONG SelectedRunAsMode;
static ULONG SelectedUserSessionId;

BOOLEAN PhMainWndInitialization(
    _In_ INT ShowCommand
    )
{
    RTL_ATOM windowAtom;
    PH_STRING_BUILDER stringBuilder;
    PH_RECTANGLE windowRectangle;

    if (PhGetIntegerSetting(L"FirstRun"))
    {
        PPH_STRING autoDbghelpPath;

        // Try to set up the dbghelp path automatically if this is the first run.
        if (autoDbghelpPath = PH_AUTO(PhFindDbghelpPath()))
            PhSetStringSetting2(L"DbgHelpPath", &autoDbghelpPath->sr);

        PhSetIntegerSetting(L"FirstRun", FALSE);
    }

    // This was added to be able to delay-load dbghelp.dll and symsrv.dll.
    PhRegisterCallback(&PhSymInitCallback, PhMwpSymInitHandler, NULL, &SymInitRegistration);

    PhMwpInitializeProviders();

    if ((windowAtom = PhMwpInitializeWindowClass()) == INVALID_ATOM)
        return FALSE;

    windowRectangle.Position = PhGetIntegerPairSetting(L"MainWindowPosition");
    windowRectangle.Size = PhGetScalableIntegerPairSetting(L"MainWindowSize", TRUE).Pair;

    // Create the window title.

    PhInitializeStringBuilder(&stringBuilder, 100);

    if (PhGetIntegerSetting(L"EnableWindowText"))
    {
        PhAppendStringBuilder2(&stringBuilder, L"Process Hacker");

        if (PhCurrentUserName)
        {
            PhAppendStringBuilder2(&stringBuilder, L" [");
            PhAppendStringBuilder(&stringBuilder, &PhCurrentUserName->sr);
            PhAppendCharStringBuilder(&stringBuilder, ']');
            if (KphIsConnected()) PhAppendCharStringBuilder(&stringBuilder, '+');
        }

        if (PhGetOwnTokenAttributes().ElevationType == TokenElevationTypeFull)
            PhAppendStringBuilder2(&stringBuilder, L" (Administrator)");
    }

    // Create the window.

    PhMainWndHandle = CreateWindow(
        MAKEINTATOM(windowAtom),
        stringBuilder.String->Buffer,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        windowRectangle.Left,
        windowRectangle.Top,
        windowRectangle.Width,
        windowRectangle.Height,
        NULL,
        NULL,
        PhInstanceHandle,
        NULL
        );
    PhDeleteStringBuilder(&stringBuilder);
    PhMainWndMenuHandle = GetMenu(PhMainWndHandle);

    if (!PhMainWndHandle)
        return FALSE;

    PhMwpInitializeMainMenu(PhMainWndMenuHandle);

    // Choose a more appropriate rectangle for the window.
    PhAdjustRectangleToWorkingArea(PhMainWndHandle, &windowRectangle);
    MoveWindow(PhMainWndHandle, windowRectangle.Left, windowRectangle.Top,
        windowRectangle.Width, windowRectangle.Height, FALSE);

    // Allow WM_PH_ACTIVATE to pass through UIPI.
    ChangeWindowMessageFilter(WM_PH_ACTIVATE, MSGFLT_ADD);

    PhMwpOnSettingChange();

    // Initialize child controls.
    PhMwpInitializeControls();

    PhMwpLoadSettings();
    PhLogInitialization();
    PhQueueItemWorkQueue(PhGetGlobalWorkQueue(), PhMwpDelayedLoadFunction, NULL);

    PhMwpSelectionChangedTabControl(-1);

    // Perform a layout.
    PhMwpOnSize();

    PhStartProviderThread(&PhPrimaryProviderThread);
    PhStartProviderThread(&PhSecondaryProviderThread);

    // See PhMwpOnTimer for more details.
    if (PhCsUpdateInterval > PH_FLUSH_PROCESS_QUERY_DATA_INTERVAL_1)
        SetTimer(PhMainWndHandle, TIMER_FLUSH_PROCESS_QUERY_DATA, PH_FLUSH_PROCESS_QUERY_DATA_INTERVAL_1, NULL);

    UpdateWindow(PhMainWndHandle);

    if ((PhStartupParameters.ShowHidden || PhGetIntegerSetting(L"StartHidden")) && PhNfTestIconMask(PH_ICON_ALL))
        ShowCommand = SW_HIDE;
    if (PhStartupParameters.ShowVisible)
        ShowCommand = SW_SHOW;

    if (PhGetIntegerSetting(L"MainWindowState") == SW_MAXIMIZE)
    {
        if (ShowCommand != SW_HIDE)
        {
            ShowCommand = SW_MAXIMIZE;
        }
        else
        {
            // We can't maximize it while having it hidden. Set it as pending.
            NeedsMaximize = TRUE;
        }
    }

    if (PhPluginsEnabled)
        PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackMainWindowShowing), IntToPtr(ShowCommand));

    if (PhStartupParameters.SelectTab)
    {
        PPH_MAIN_TAB_PAGE page = PhMwpFindPage(&PhStartupParameters.SelectTab->sr);

        if (page)
            PhMwpSelectPage(page->Index);
    }
    else
    {
        if (PhGetIntegerSetting(L"MainWindowTabRestoreEnabled"))
            PhMwpSelectPage(PhGetIntegerSetting(L"MainWindowTabRestoreIndex"));
    }

    if (PhStartupParameters.SysInfo)
        PhShowSystemInformationDialog(PhStartupParameters.SysInfo->Buffer);

    if (ShowCommand != SW_HIDE)
        ShowWindow(PhMainWndHandle, ShowCommand);

    if (PhGetIntegerSetting(L"MiniInfoWindowPinned"))
        PhPinMiniInformation(MiniInfoManualPinType, 1, 0, PH_MINIINFO_LOAD_POSITION, NULL, NULL);

    return TRUE;
}

LRESULT CALLBACK PhMwpWndProc(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_DESTROY:
        {
            PhMwpOnDestroy();
        }
        break;
    case WM_ENDSESSION:
        {
            PhMwpOnEndSession();
        }
        break;
    case WM_SETTINGCHANGE:
        {
            PhMwpOnSettingChange();
        }
        break;
    case WM_COMMAND:
        {
            PhMwpOnCommand(GET_WM_COMMAND_ID(wParam, lParam));
        }
        break;
    case WM_SHOWWINDOW:
        {
            PhMwpOnShowWindow(!!wParam, (ULONG)lParam);
        }
        break;
    case WM_SYSCOMMAND:
        {
            if (PhMwpOnSysCommand((ULONG)wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
                return 0;
        }
        break;
    case WM_MENUCOMMAND:
        {
            PhMwpOnMenuCommand((ULONG)wParam, (HMENU)lParam);
        }
        break;
    case WM_INITMENUPOPUP:
        {
            PhMwpOnInitMenuPopup((HMENU)wParam, LOWORD(lParam), !!HIWORD(lParam));
        }
        break;
    case WM_SIZE:
        {
            PhMwpOnSize();
        }
        break;
    case WM_SIZING:
        {
            PhMwpOnSizing((ULONG)wParam, (PRECT)lParam);
        }
        break;
    case WM_SETFOCUS:
        {
            PhMwpOnSetFocus();
        }
        break;
    case WM_TIMER:
        {
            PhMwpOnTimer((ULONG)wParam);
        }
        break;
    case WM_NOTIFY:
        {
            LRESULT result;

            if (PhMwpOnNotify((NMHDR *)lParam, &result))
                return result;
        }
        break;
    }

    if (uMsg >= WM_PH_FIRST && uMsg <= WM_PH_LAST)
    {
        return PhMwpOnUserMessage(uMsg, wParam, lParam);
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

RTL_ATOM PhMwpInitializeWindowClass(
    VOID
    )
{
    WNDCLASSEX wcex;
    PPH_STRING className;
    
    memset(&wcex, 0, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = 0;
    wcex.lpfnWndProc = PhMwpWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = PhInstanceHandle;
    className = PhaGetStringSetting(L"MainWindowClassName");
    wcex.lpszClassName = PhGetStringOrDefault(className, L"MainWindowClassName");
    wcex.lpszMenuName = MAKEINTRESOURCE(IDR_MAINWND);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hIcon = PH_LOAD_SHARED_ICON_LARGE(PhInstanceHandle, MAKEINTRESOURCE(IDI_PROCESSHACKER));
    wcex.hIconSm = PH_LOAD_SHARED_ICON_SMALL(PhInstanceHandle, MAKEINTRESOURCE(IDI_PROCESSHACKER));

    return RegisterClassEx(&wcex);
}

VOID PhMwpInitializeProviders(
    VOID
    )
{
    ULONG interval;

    interval = PhGetIntegerSetting(L"UpdateInterval");

    if (interval == 0)
    {
        interval = 1000;
        PH_SET_INTEGER_CACHED_SETTING(UpdateInterval, interval);
    }

    PhInitializeProviderThread(&PhPrimaryProviderThread, interval);
    PhInitializeProviderThread(&PhSecondaryProviderThread, interval);

    PhRegisterProvider(&PhPrimaryProviderThread, PhProcessProviderUpdate, NULL, &PhMwpProcessProviderRegistration);
    PhSetEnabledProvider(&PhMwpProcessProviderRegistration, TRUE);
    PhRegisterProvider(&PhPrimaryProviderThread, PhServiceProviderUpdate, NULL, &PhMwpServiceProviderRegistration);
    PhSetEnabledProvider(&PhMwpServiceProviderRegistration, TRUE);
    PhRegisterProvider(&PhPrimaryProviderThread, PhNetworkProviderUpdate, NULL, &PhMwpNetworkProviderRegistration);
}

VOID PhMwpApplyUpdateInterval(
    _In_ ULONG Interval
    )
{
    PhSetIntervalProviderThread(&PhPrimaryProviderThread, Interval);
    PhSetIntervalProviderThread(&PhSecondaryProviderThread, Interval);

    if (Interval > PH_FLUSH_PROCESS_QUERY_DATA_INTERVAL_LONG_TERM)
        SetTimer(PhMainWndHandle, TIMER_FLUSH_PROCESS_QUERY_DATA, PH_FLUSH_PROCESS_QUERY_DATA_INTERVAL_LONG_TERM, NULL);
    else
        KillTimer(PhMainWndHandle, TIMER_FLUSH_PROCESS_QUERY_DATA); // Might not exist
}

VOID PhMwpInitializeControls(
    VOID
    )
{
    ULONG thinRows;

    TabControlHandle = CreateWindow(
        WC_TABCONTROL,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_MULTILINE,
        0,
        0,
        3,
        3,
        PhMainWndHandle,
        NULL,
        PhInstanceHandle,
        NULL
        );
    SendMessage(TabControlHandle, WM_SETFONT, (WPARAM)PhApplicationFont, FALSE);
    BringWindowToTop(TabControlHandle);

    thinRows = PhGetIntegerSetting(L"ThinRows") ? TN_STYLE_THIN_ROWS : 0;

    PhMwpProcessTreeNewHandle = CreateWindow(
        PH_TREENEW_CLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_BORDER | TN_STYLE_ICONS | TN_STYLE_DOUBLE_BUFFERED | TN_STYLE_ANIMATE_DIVIDER | thinRows,
        0,
        0,
        3,
        3,
        PhMainWndHandle,
        NULL,
        PhInstanceHandle,
        NULL
        );
    BringWindowToTop(PhMwpProcessTreeNewHandle);

    PhMwpServiceTreeNewHandle = CreateWindow(
        PH_TREENEW_CLASSNAME,
        NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_BORDER | TN_STYLE_ICONS | TN_STYLE_DOUBLE_BUFFERED | thinRows,
        0,
        0,
        3,
        3,
        PhMainWndHandle,
        NULL,
        PhInstanceHandle,
        NULL
        );
    BringWindowToTop(PhMwpServiceTreeNewHandle);

    PhMwpNetworkTreeNewHandle = CreateWindow(
        PH_TREENEW_CLASSNAME,
        NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_BORDER | TN_STYLE_ICONS | TN_STYLE_DOUBLE_BUFFERED | thinRows,
        0,
        0,
        3,
        3,
        PhMainWndHandle,
        NULL,
        PhInstanceHandle,
        NULL
        );
    BringWindowToTop(PhMwpNetworkTreeNewHandle);

    PageList = PhCreateList(10);

    PhMwpCreateInternalPage(L"Processes", 0, PhMwpProcessesPageCallback);
    PhProcessTreeListInitialization();
    PhInitializeProcessTreeList(PhMwpProcessTreeNewHandle);

    PhMwpCreateInternalPage(L"Services", 0, PhMwpServicesPageCallback);
    PhServiceTreeListInitialization();
    PhInitializeServiceTreeList(PhMwpServiceTreeNewHandle);

    PhMwpCreateInternalPage(L"Network", 0, PhMwpNetworkPageCallback);
    PhNetworkTreeListInitialization();
    PhInitializeNetworkTreeList(PhMwpNetworkTreeNewHandle);

    CurrentPage = PageList->Items[0];
}

NTSTATUS PhMwpDelayedLoadFunction(
    _In_ PVOID Parameter
    )
{
    PhNfLoadStage2();

    // Make sure we get closed late in the shutdown process.
    SetProcessShutdownParameters(0x100, 0);

    DelayedLoadCompleted = TRUE;
    //PostMessage(PhMainWndHandle, WM_PH_DELAYED_LOAD_COMPLETED, 0, 0);

    return STATUS_SUCCESS;
}

VOID PhMwpOnDestroy(
    VOID
    )
{
    PhSetIntegerSetting(L"MainWindowTabRestoreIndex", TabCtrl_GetCurSel(TabControlHandle));

    // Notify pages and plugins that we are shutting down.

    PhMwpNotifyAllPages(MainTabPageDestroy, NULL, NULL);

    if (PhPluginsEnabled)
        PhUnloadPlugins();

    if (!PhMainWndExiting)
        ProcessHacker_SaveAllSettings(PhMainWndHandle);

    PhNfUninitialization();

    PostQuitMessage(0);
}

VOID PhMwpOnEndSession(
    VOID
    )
{
    PhMwpOnDestroy();
}

VOID PhMwpOnSettingChange(
    VOID
    )
{
    if (PhApplicationFont)
        DeleteObject(PhApplicationFont);

    PhInitializeFont();

    SendMessage(TabControlHandle, WM_SETFONT, (WPARAM)PhApplicationFont, FALSE);
}

VOID PhMwpOnCommand(
    _In_ ULONG Id
    )
{
    switch (Id)
    {
    case ID_ESC_EXIT:
        {
            if (PhGetIntegerSetting(L"HideOnClose"))
            {
                if (PhNfTestIconMask(PH_ICON_ALL))
                    ShowWindow(PhMainWndHandle, SW_HIDE);
            }
            else if (PhGetIntegerSetting(L"CloseOnEscape"))
            {
                ProcessHacker_Destroy(PhMainWndHandle);
            }
        }
        break;
    case ID_HACKER_RUN:
        {
            if (RunFileDlg)
            {
                SelectedRunAsMode = 0;
                RunFileDlg(PhMainWndHandle, NULL, NULL, NULL, NULL, 0);
            }
        }
        break;
    case ID_HACKER_RUNASADMINISTRATOR:
        {
            if (RunFileDlg)
            {
                SelectedRunAsMode = RUNAS_MODE_ADMIN;
                RunFileDlg(
                    PhMainWndHandle,
                    NULL,
                    NULL,
                    NULL,
                    L"Type the name of a program that will be opened under alternate credentials.",
                    0
                    );
            }
        }
        break;
    case ID_HACKER_RUNASLIMITEDUSER:
        {
            if (RunFileDlg)
            {
                SelectedRunAsMode = RUNAS_MODE_LIMITED;
                RunFileDlg(
                    PhMainWndHandle,
                    NULL,
                    NULL,
                    NULL,
                    L"Type the name of a program that will be opened under standard user privileges.",
                    0
                    );
            }
        }
        break;
    case ID_HACKER_RUNAS:
        {
            PhShowRunAsDialog(PhMainWndHandle, NULL);
        }
        break;
    case ID_HACKER_SHOWDETAILSFORALLPROCESSES:
        {
            ProcessHacker_PrepareForEarlyShutdown(PhMainWndHandle);

            if (PhShellProcessHacker(
                PhMainWndHandle,
                L"-v",
                SW_SHOW,
                PH_SHELL_EXECUTE_ADMIN,
                PH_SHELL_APP_PROPAGATE_PARAMETERS | PH_SHELL_APP_PROPAGATE_PARAMETERS_IGNORE_VISIBILITY,
                0,
                NULL
                ))
            {
                ProcessHacker_Destroy(PhMainWndHandle);
            }
            else
            {
                ProcessHacker_CancelEarlyShutdown(PhMainWndHandle);
            }
        }
        break;
    case ID_HACKER_SAVE:
        {
            static PH_FILETYPE_FILTER filters[] =
            {
                { L"Text files (*.txt;*.log)", L"*.txt;*.log" },
                { L"Comma-separated values (*.csv)", L"*.csv" },
                { L"All files (*.*)", L"*.*" }
            };
            PVOID fileDialog = PhCreateSaveFileDialog();
            PH_FORMAT format[3];

            PhInitFormatS(&format[0], L"Process Hacker ");
            PhInitFormatSR(&format[1], CurrentPage->Name);
            PhInitFormatS(&format[2], L".txt");

            PhSetFileDialogFilter(fileDialog, filters, sizeof(filters) / sizeof(PH_FILETYPE_FILTER));
            PhSetFileDialogFileName(fileDialog, PH_AUTO_T(PH_STRING, PhFormat(format, 3, 60))->Buffer);

            if (PhShowFileDialog(PhMainWndHandle, fileDialog))
            {
                NTSTATUS status;
                PPH_STRING fileName;
                ULONG filterIndex;
                PPH_FILE_STREAM fileStream;

                fileName = PH_AUTO(PhGetFileDialogFileName(fileDialog));
                filterIndex = PhGetFileDialogFilterIndex(fileDialog);

                if (NT_SUCCESS(status = PhCreateFileStream(
                    &fileStream,
                    fileName->Buffer,
                    FILE_GENERIC_WRITE,
                    FILE_SHARE_READ,
                    FILE_OVERWRITE_IF,
                    0
                    )))
                {
                    ULONG mode;
                    PH_MAIN_TAB_PAGE_EXPORT_CONTENT exportContent;

                    if (filterIndex == 2)
                        mode = PH_EXPORT_MODE_CSV;
                    else
                        mode = PH_EXPORT_MODE_TABS;

                    PhWriteStringAsUtf8FileStream(fileStream, &PhUnicodeByteOrderMark);
                    PhWritePhTextHeader(fileStream);

                    exportContent.FileStream = fileStream;
                    exportContent.Mode = mode;
                    CurrentPage->Callback(CurrentPage, MainTabPageExportContent, &exportContent, NULL);

                    PhDereferenceObject(fileStream);
                }

                if (!NT_SUCCESS(status))
                    PhShowStatus(PhMainWndHandle, L"Unable to create the file", status, 0);
            }

            PhFreeFileDialog(fileDialog);
        }
        break;
    case ID_HACKER_FINDHANDLESORDLLS:
        {
            PhShowFindObjectsDialog();
        }
        break;
    case ID_HACKER_OPTIONS:
        {
            PhShowOptionsDialog(PhMainWndHandle);
        }
        break;
    case ID_HACKER_PLUGINS:
        {
            PhShowPluginsDialog(PhMainWndHandle);
        }
        break;
    case ID_COMPUTER_LOCK:
    case ID_COMPUTER_LOGOFF:
    case ID_COMPUTER_SLEEP:
    case ID_COMPUTER_HIBERNATE:
    case ID_COMPUTER_RESTART:
    case ID_COMPUTER_RESTARTBOOTOPTIONS:
    case ID_COMPUTER_SHUTDOWN:
    case ID_COMPUTER_SHUTDOWNHYBRID:
        PhMwpExecuteComputerCommand(Id);
        break;
    case ID_HACKER_EXIT:
        ProcessHacker_Destroy(PhMainWndHandle);
        break;
    case ID_VIEW_SYSTEMINFORMATION:
        PhShowSystemInformationDialog(NULL);
        break;
    case ID_TRAYICONS_CPUHISTORY:
    case ID_TRAYICONS_CPUUSAGE:
    case ID_TRAYICONS_IOHISTORY:
    case ID_TRAYICONS_COMMITHISTORY:
    case ID_TRAYICONS_PHYSICALMEMORYHISTORY:
        {
            ULONG i;

            switch (Id)
            {
            case ID_TRAYICONS_CPUHISTORY:
                i = PH_ICON_CPU_HISTORY;
                break;
            case ID_TRAYICONS_CPUUSAGE:
                i = PH_ICON_CPU_USAGE;
                break;
            case ID_TRAYICONS_IOHISTORY:
                i = PH_ICON_IO_HISTORY;
                break;
            case ID_TRAYICONS_COMMITHISTORY:
                i = PH_ICON_COMMIT_HISTORY;
                break;
            case ID_TRAYICONS_PHYSICALMEMORYHISTORY:
                i = PH_ICON_PHYSICAL_HISTORY;
                break;
            }

            PhNfSetVisibleIcon(i, !PhNfTestIconMask(i));
        }
        break;
    case ID_VIEW_HIDEPROCESSESFROMOTHERUSERS:
        {
            PhMwpToggleCurrentUserProcessTreeFilter();
        }
        break;
    case ID_VIEW_HIDESIGNEDPROCESSES:
        {
            PhMwpToggleSignedProcessTreeFilter();
        }
        break;
    case ID_VIEW_SCROLLTONEWPROCESSES:
        {
            PH_SET_INTEGER_CACHED_SETTING(ScrollToNewProcesses, !PhCsScrollToNewProcesses);
        }
        break;
    case ID_VIEW_SHOWCPUBELOW001:
        {
            PH_SET_INTEGER_CACHED_SETTING(ShowCpuBelow001, !PhCsShowCpuBelow001);
            PhInvalidateAllProcessNodes();
        }
        break;
    case ID_VIEW_HIDEDRIVERSERVICES:
        {
            PhMwpToggleDriverServiceTreeFilter();
        }
        break;
    case ID_VIEW_ALWAYSONTOP:
        {
            AlwaysOnTop = !AlwaysOnTop;
            SetWindowPos(PhMainWndHandle, AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            PhSetIntegerSetting(L"MainWindowAlwaysOnTop", AlwaysOnTop);
        }
        break;
    case ID_OPACITY_10:
    case ID_OPACITY_20:
    case ID_OPACITY_30:
    case ID_OPACITY_40:
    case ID_OPACITY_50:
    case ID_OPACITY_60:
    case ID_OPACITY_70:
    case ID_OPACITY_80:
    case ID_OPACITY_90:
    case ID_OPACITY_OPAQUE:
        {
            ULONG opacity;

            opacity = PH_ID_TO_OPACITY(Id);
            PhSetIntegerSetting(L"MainWindowOpacity", opacity);
            PhSetWindowOpacity(PhMainWndHandle, opacity);
        }
        break;
    case ID_VIEW_REFRESH:
        {
            PhBoostProvider(&PhMwpProcessProviderRegistration, NULL);
            PhBoostProvider(&PhMwpServiceProviderRegistration, NULL);
        }
        break;
    case ID_UPDATEINTERVAL_FAST:
    case ID_UPDATEINTERVAL_NORMAL:
    case ID_UPDATEINTERVAL_BELOWNORMAL:
    case ID_UPDATEINTERVAL_SLOW:
    case ID_UPDATEINTERVAL_VERYSLOW:
        {
            ULONG interval;

            switch (Id)
            {
            case ID_UPDATEINTERVAL_FAST:
                interval = 500;
                break;
            case ID_UPDATEINTERVAL_NORMAL:
                interval = 1000;
                break;
            case ID_UPDATEINTERVAL_BELOWNORMAL:
                interval = 2000;
                break;
            case ID_UPDATEINTERVAL_SLOW:
                interval = 5000;
                break;
            case ID_UPDATEINTERVAL_VERYSLOW:
                interval = 10000;
                break;
            }

            PH_SET_INTEGER_CACHED_SETTING(UpdateInterval, interval);
            PhMwpApplyUpdateInterval(interval);
        }
        break;
    case ID_VIEW_UPDATEAUTOMATICALLY:
        {
            PhMwpUpdateAutomatically = !PhMwpUpdateAutomatically;
            PhMwpNotifyAllPages(MainTabPageUpdateAutomaticallyChanged, (PVOID)PhMwpUpdateAutomatically, NULL);
        }
        break;
    case ID_TOOLS_CREATESERVICE:
        {
            PhShowCreateServiceDialog(PhMainWndHandle);
        }
        break;
    case ID_TOOLS_HIDDENPROCESSES:
        {
            PhShowHiddenProcessesDialog();
        }
        break;
    case ID_TOOLS_INSPECTEXECUTABLEFILE:
        {
            static PH_FILETYPE_FILTER filters[] =
            {
                { L"Executable files (*.exe;*.dll;*.ocx;*.sys;*.scr;*.cpl)", L"*.exe;*.dll;*.ocx;*.sys;*.scr;*.cpl" },
                { L"All files (*.*)", L"*.*" }
            };
            PVOID fileDialog = PhCreateOpenFileDialog();

            PhSetFileDialogFilter(fileDialog, filters, sizeof(filters) / sizeof(PH_FILETYPE_FILTER));

            if (PhShowFileDialog(PhMainWndHandle, fileDialog))
            {
                PhShellExecuteUserString(
                    PhMainWndHandle,
                    L"ProgramInspectExecutables",
                    PH_AUTO_T(PH_STRING, PhGetFileDialogFileName(fileDialog))->Buffer,
                    FALSE,
                    L"Make sure the PE Viewer executable file is present."
                    );
            }

            PhFreeFileDialog(fileDialog);
        }
        break;
    case ID_TOOLS_PAGEFILES:
        {
            PhShowPagefilesDialog(PhMainWndHandle);
        }
        break;
    case ID_TOOLS_STARTTASKMANAGER:
        {
            PPH_STRING systemDirectory;
            PPH_STRING taskmgrFileName;

            systemDirectory = PH_AUTO(PhGetSystemDirectory());
            taskmgrFileName = PH_AUTO(PhConcatStrings2(systemDirectory->Buffer, L"\\taskmgr.exe"));

            if (WindowsVersion >= WINDOWS_8 && !PhGetOwnTokenAttributes().Elevated)
            {
                if (PhUiConnectToPhSvc(PhMainWndHandle, FALSE))
                {
                    PhSvcCallCreateProcessIgnoreIfeoDebugger(taskmgrFileName->Buffer);
                    PhUiDisconnectFromPhSvc();
                }
            }
            else
            {
                PhCreateProcessIgnoreIfeoDebugger(taskmgrFileName->Buffer);
            }
        }
        break;
    case ID_USER_CONNECT:
        {
            PhUiConnectSession(PhMainWndHandle, SelectedUserSessionId);
        }
        break;
    case ID_USER_DISCONNECT:
        {
            PhUiDisconnectSession(PhMainWndHandle, SelectedUserSessionId);
        }
        break;
    case ID_USER_LOGOFF:
        {
            PhUiLogoffSession(PhMainWndHandle, SelectedUserSessionId);
        }
        break;
    case ID_USER_REMOTECONTROL:
        {
            PhShowSessionShadowDialog(PhMainWndHandle, SelectedUserSessionId);
        }
        break;
    case ID_USER_SENDMESSAGE:
        {
            PhShowSessionSendMessageDialog(PhMainWndHandle, SelectedUserSessionId);
        }
        break;
    case ID_USER_PROPERTIES:
        {
            PhShowSessionProperties(PhMainWndHandle, SelectedUserSessionId);
        }
        break;
    case ID_HELP_LOG:
        {
            PhShowLogDialog();
        }
        break;
    case ID_HELP_DONATE:
        {
            PhShellExecute(PhMainWndHandle, L"https://sourceforge.net/project/project_donations.php?group_id=242527", NULL);
        }
        break;
    case ID_HELP_DEBUGCONSOLE:
        {
            PhShowDebugConsole();
        }
        break;
    case ID_HELP_ABOUT:
        {
            PhShowAboutDialog(PhMainWndHandle);
        }
        break;
    case ID_PROCESS_TERMINATE:
        {
            PPH_PROCESS_ITEM *processes;
            ULONG numberOfProcesses;

            PhGetSelectedProcessItems(&processes, &numberOfProcesses);
            PhReferenceObjects(processes, numberOfProcesses);

            if (PhUiTerminateProcesses(PhMainWndHandle, processes, numberOfProcesses))
                PhDeselectAllProcessNodes();

            PhDereferenceObjects(processes, numberOfProcesses);
            PhFree(processes);
        }
        break;
    case ID_PROCESS_TERMINATETREE:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);

                if (PhUiTerminateTreeProcess(PhMainWndHandle, processItem))
                    PhDeselectAllProcessNodes();

                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_PROCESS_SUSPEND:
        {
            PPH_PROCESS_ITEM *processes;
            ULONG numberOfProcesses;

            PhGetSelectedProcessItems(&processes, &numberOfProcesses);
            PhReferenceObjects(processes, numberOfProcesses);
            PhUiSuspendProcesses(PhMainWndHandle, processes, numberOfProcesses);
            PhDereferenceObjects(processes, numberOfProcesses);
            PhFree(processes);
        }
        break;
    case ID_PROCESS_RESUME:
        {
            PPH_PROCESS_ITEM *processes;
            ULONG numberOfProcesses;

            PhGetSelectedProcessItems(&processes, &numberOfProcesses);
            PhReferenceObjects(processes, numberOfProcesses);
            PhUiResumeProcesses(PhMainWndHandle, processes, numberOfProcesses);
            PhDereferenceObjects(processes, numberOfProcesses);
            PhFree(processes);
        }
        break;
    case ID_PROCESS_RESTART:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);

                if (PhUiRestartProcess(PhMainWndHandle, processItem))
                    PhDeselectAllProcessNodes();

                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_PROCESS_CREATEDUMPFILE:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);
                PhUiCreateDumpFileProcess(PhMainWndHandle, processItem);
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_PROCESS_DEBUG:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);
                PhUiDebugProcess(PhMainWndHandle, processItem);
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_PROCESS_VIRTUALIZATION:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);
                PhUiSetVirtualizationProcess(
                    PhMainWndHandle,
                    processItem,
                    !PhMwpSelectedProcessVirtualizationEnabled
                    );
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_PROCESS_AFFINITY:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);
                PhShowProcessAffinityDialog(PhMainWndHandle, processItem, NULL);
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_MISCELLANEOUS_DETACHFROMDEBUGGER:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);
                PhUiDetachFromDebuggerProcess(PhMainWndHandle, processItem);
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_MISCELLANEOUS_GDIHANDLES:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);
                PhShowGdiHandlesDialog(PhMainWndHandle, processItem);
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_MISCELLANEOUS_INJECTDLL:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhReferenceObject(processItem);
                PhUiInjectDllProcess(PhMainWndHandle, processItem);
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_PAGEPRIORITY_VERYLOW:
    case ID_PAGEPRIORITY_LOW:
    case ID_PAGEPRIORITY_MEDIUM:
    case ID_PAGEPRIORITY_BELOWNORMAL:
    case ID_PAGEPRIORITY_NORMAL:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                ULONG pagePriority;

                switch (Id)
                {
                    case ID_PAGEPRIORITY_VERYLOW:
                        pagePriority = MEMORY_PRIORITY_VERY_LOW;
                        break;
                    case ID_PAGEPRIORITY_LOW:
                        pagePriority = MEMORY_PRIORITY_LOW;
                        break;
                    case ID_PAGEPRIORITY_MEDIUM:
                        pagePriority = MEMORY_PRIORITY_MEDIUM;
                        break;
                    case ID_PAGEPRIORITY_BELOWNORMAL:
                        pagePriority = MEMORY_PRIORITY_BELOW_NORMAL;
                        break;
                    case ID_PAGEPRIORITY_NORMAL:
                        pagePriority = MEMORY_PRIORITY_NORMAL;
                        break;
                }

                PhReferenceObject(processItem);
                PhUiSetPagePriorityProcess(PhMainWndHandle, processItem, pagePriority);
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_MISCELLANEOUS_REDUCEWORKINGSET:
        {
            PPH_PROCESS_ITEM *processes;
            ULONG numberOfProcesses;

            PhGetSelectedProcessItems(&processes, &numberOfProcesses);
            PhReferenceObjects(processes, numberOfProcesses);
            PhUiReduceWorkingSetProcesses(PhMainWndHandle, processes, numberOfProcesses);
            PhDereferenceObjects(processes, numberOfProcesses);
            PhFree(processes);
        }
        break;
    case ID_MISCELLANEOUS_RUNAS:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem && processItem->FileName)
            {
                PhSetStringSetting2(L"RunAsProgram", &processItem->FileName->sr);
                PhShowRunAsDialog(PhMainWndHandle, NULL);
            }
        }
        break;
    case ID_MISCELLANEOUS_RUNASTHISUSER:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhShowRunAsDialog(PhMainWndHandle, processItem->ProcessId);
            }
        }
        break;
    case ID_PRIORITY_REALTIME:
    case ID_PRIORITY_HIGH:
    case ID_PRIORITY_ABOVENORMAL:
    case ID_PRIORITY_NORMAL:
    case ID_PRIORITY_BELOWNORMAL:
    case ID_PRIORITY_IDLE:
        {
            PPH_PROCESS_ITEM *processes;
            ULONG numberOfProcesses;

            PhGetSelectedProcessItems(&processes, &numberOfProcesses);
            PhReferenceObjects(processes, numberOfProcesses);
            PhMwpExecuteProcessPriorityCommand(Id, processes, numberOfProcesses);
            PhDereferenceObjects(processes, numberOfProcesses);
            PhFree(processes);
        }
        break;
    case ID_IOPRIORITY_VERYLOW:
    case ID_IOPRIORITY_LOW:
    case ID_IOPRIORITY_NORMAL:
    case ID_IOPRIORITY_HIGH:
        {
            PPH_PROCESS_ITEM *processes;
            ULONG numberOfProcesses;

            PhGetSelectedProcessItems(&processes, &numberOfProcesses);
            PhReferenceObjects(processes, numberOfProcesses);
            PhMwpExecuteProcessIoPriorityCommand(Id, processes, numberOfProcesses);
            PhDereferenceObjects(processes, numberOfProcesses);
            PhFree(processes);
        }
        break;
    case ID_WINDOW_BRINGTOFRONT:
        {
            if (IsWindow(PhMwpSelectedProcessWindowHandle))
            {
                WINDOWPLACEMENT placement = { sizeof(placement) };

                GetWindowPlacement(PhMwpSelectedProcessWindowHandle, &placement);

                if (placement.showCmd == SW_MINIMIZE)
                    ShowWindowAsync(PhMwpSelectedProcessWindowHandle, SW_RESTORE);
                else
                    SetForegroundWindow(PhMwpSelectedProcessWindowHandle);
            }
        }
        break;
    case ID_WINDOW_RESTORE:
        {
            if (IsWindow(PhMwpSelectedProcessWindowHandle))
            {
                ShowWindowAsync(PhMwpSelectedProcessWindowHandle, SW_RESTORE);
            }
        }
        break;
    case ID_WINDOW_MINIMIZE:
        {
            if (IsWindow(PhMwpSelectedProcessWindowHandle))
            {
                ShowWindowAsync(PhMwpSelectedProcessWindowHandle, SW_MINIMIZE);
            }
        }
        break;
    case ID_WINDOW_MAXIMIZE:
        {
            if (IsWindow(PhMwpSelectedProcessWindowHandle))
            {
                ShowWindowAsync(PhMwpSelectedProcessWindowHandle, SW_MAXIMIZE);
            }
        }
        break;
    case ID_WINDOW_CLOSE:
        {
            if (IsWindow(PhMwpSelectedProcessWindowHandle))
            {
                PostMessage(PhMwpSelectedProcessWindowHandle, WM_CLOSE, 0, 0);
            }
        }
        break;
    case ID_PROCESS_OPENFILELOCATION:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem && processItem->FileName)
            {
                PhReferenceObject(processItem);
                PhShellExecuteUserString(
                    PhMainWndHandle,
                    L"FileBrowseExecutable",
                    processItem->FileName->Buffer,
                    FALSE,
                    L"Make sure the Explorer executable file is present."
                    );
                PhDereferenceObject(processItem);
            }
        }
        break;
    case ID_PROCESS_SEARCHONLINE:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                PhSearchOnlineString(PhMainWndHandle, processItem->ProcessName->Buffer);
            }
        }
        break;
    case ID_PROCESS_PROPERTIES:
        {
            PPH_PROCESS_ITEM processItem = PhGetSelectedProcessItem();

            if (processItem)
            {
                // No reference needed; no messages pumped.
                PhMwpShowProcessProperties(processItem);
            }
        }
        break;
    case ID_PROCESS_COPY:
        {
            PhCopyProcessTree();
        }
        break;
    case ID_SERVICE_GOTOPROCESS:
        {
            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();
            PPH_PROCESS_NODE processNode;

            if (serviceItem)
            {
                if (processNode = PhFindProcessNode(serviceItem->ProcessId))
                {
                    PhMwpSelectPage(PhMwpProcessesPage->Index);
                    SetFocus(PhMwpProcessTreeNewHandle);
                    PhSelectAndEnsureVisibleProcessNode(processNode);
                }
            }
        }
        break;
    case ID_SERVICE_START:
        {
            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();

            if (serviceItem)
            {
                PhReferenceObject(serviceItem);
                PhUiStartService(PhMainWndHandle, serviceItem);
                PhDereferenceObject(serviceItem);
            }
        }
        break;
    case ID_SERVICE_CONTINUE:
        {
            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();

            if (serviceItem)
            {
                PhReferenceObject(serviceItem);
                PhUiContinueService(PhMainWndHandle, serviceItem);
                PhDereferenceObject(serviceItem);
            }
        }
        break;
    case ID_SERVICE_PAUSE:
        {
            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();

            if (serviceItem)
            {
                PhReferenceObject(serviceItem);
                PhUiPauseService(PhMainWndHandle, serviceItem);
                PhDereferenceObject(serviceItem);
            }
        }
        break;
    case ID_SERVICE_STOP:
        {
            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();

            if (serviceItem)
            {
                PhReferenceObject(serviceItem);
                PhUiStopService(PhMainWndHandle, serviceItem);
                PhDereferenceObject(serviceItem);
            }
        }
        break;
    case ID_SERVICE_DELETE:
        {
            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();

            if (serviceItem)
            {
                PhReferenceObject(serviceItem);

                if (PhUiDeleteService(PhMainWndHandle, serviceItem))
                    PhDeselectAllServiceNodes();

                PhDereferenceObject(serviceItem);
            }
        }
        break;
    case ID_SERVICE_OPENKEY:
        {
            static PH_STRINGREF servicesKeyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Services\\");
            static PH_STRINGREF hklm = PH_STRINGREF_INIT(L"HKLM\\");

            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();

            if (serviceItem)
            {
                HANDLE keyHandle;
                PPH_STRING serviceKeyName = PH_AUTO(PhConcatStringRef2(&servicesKeyName, &serviceItem->Name->sr));

                if (NT_SUCCESS(PhOpenKey(
                    &keyHandle,
                    KEY_READ,
                    PH_KEY_LOCAL_MACHINE,
                    &serviceKeyName->sr,
                    0
                    )))
                {
                    PPH_STRING hklmServiceKeyName;

                    hklmServiceKeyName = PH_AUTO(PhConcatStringRef2(&hklm, &serviceKeyName->sr));
                    PhShellOpenKey2(PhMainWndHandle, hklmServiceKeyName);
                    NtClose(keyHandle);
                }
            }
        }
        break;
    case ID_SERVICE_OPENFILELOCATION:
        {
            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();
            SC_HANDLE serviceHandle;

            if (serviceItem && (serviceHandle = PhOpenService(serviceItem->Name->Buffer, SERVICE_QUERY_CONFIG)))
            {
                PPH_STRING fileName;

                if (fileName = PhGetServiceRelevantFileName(&serviceItem->Name->sr, serviceHandle))
                {
                    PhShellExecuteUserString(
                        PhMainWndHandle,
                        L"FileBrowseExecutable",
                        fileName->Buffer,
                        FALSE,
                        L"Make sure the Explorer executable file is present."
                        );
                    PhDereferenceObject(fileName);
                }

                CloseServiceHandle(serviceHandle);
            }
        }
        break;
    case ID_SERVICE_PROPERTIES:
        {
            PPH_SERVICE_ITEM serviceItem = PhGetSelectedServiceItem();

            if (serviceItem)
            {
                // The object relies on the list view reference, which could
                // disappear if we don't reference the object here.
                PhReferenceObject(serviceItem);
                PhShowServiceProperties(PhMainWndHandle, serviceItem);
                PhDereferenceObject(serviceItem);
            }
        }
        break;
    case ID_SERVICE_COPY:
        {
            PhCopyServiceList();
        }
        break;
    case ID_NETWORK_GOTOPROCESS:
        {
            PPH_NETWORK_ITEM networkItem = PhGetSelectedNetworkItem();
            PPH_PROCESS_NODE processNode;

            if (networkItem)
            {
                if (processNode = PhFindProcessNode(networkItem->ProcessId))
                {
                    PhMwpSelectPage(PhMwpProcessesPage->Index);
                    SetFocus(PhMwpProcessTreeNewHandle);
                    PhSelectAndEnsureVisibleProcessNode(processNode);
                }
            }
        }
        break;
    case ID_NETWORK_GOTOSERVICE:
        {
            PPH_NETWORK_ITEM networkItem = PhGetSelectedNetworkItem();
            PPH_SERVICE_ITEM serviceItem;

            if (networkItem && networkItem->OwnerName)
            {
                if (serviceItem = PhReferenceServiceItem(networkItem->OwnerName->Buffer))
                {
                    PhMwpSelectPage(PhMwpServicesPage->Index);
                    SetFocus(PhMwpServiceTreeNewHandle);
                    ProcessHacker_SelectServiceItem(PhMainWndHandle, serviceItem);

                    PhDereferenceObject(serviceItem);
                }
            }
        }
        break;
    case ID_NETWORK_CLOSE:
        {
            PPH_NETWORK_ITEM *networkItems;
            ULONG numberOfNetworkItems;

            PhGetSelectedNetworkItems(&networkItems, &numberOfNetworkItems);
            PhReferenceObjects(networkItems, numberOfNetworkItems);

            if (PhUiCloseConnections(PhMainWndHandle, networkItems, numberOfNetworkItems))
                PhDeselectAllNetworkNodes();

            PhDereferenceObjects(networkItems, numberOfNetworkItems);
            PhFree(networkItems);
        }
        break;
    case ID_NETWORK_COPY:
        {
            PhCopyNetworkList();
        }
        break;
    case ID_TAB_NEXT:
        {
            ULONG selectedIndex = TabCtrl_GetCurSel(TabControlHandle);

            if (selectedIndex != PageList->Count - 1)
                selectedIndex++;
            else
                selectedIndex = 0;

            PhMwpSelectPage(selectedIndex);
        }
        break;
    case ID_TAB_PREV:
        {
            ULONG selectedIndex = TabCtrl_GetCurSel(TabControlHandle);

            if (selectedIndex != 0)
                selectedIndex--;
            else
                selectedIndex = PageList->Count - 1;

            PhMwpSelectPage(selectedIndex);
        }
        break;
    }
}

VOID PhMwpOnShowWindow(
    _In_ BOOLEAN Showing,
    _In_ ULONG State
    )
{
    if (NeedsMaximize)
    {
        ShowWindow(PhMainWndHandle, SW_MAXIMIZE);
        NeedsMaximize = FALSE;
    }
}

BOOLEAN PhMwpOnSysCommand(
    _In_ ULONG Type,
    _In_ LONG CursorScreenX,
    _In_ LONG CursorScreenY
    )
{
    switch (Type)
    {
    case SC_CLOSE:
        {
            if (PhGetIntegerSetting(L"HideOnClose") && PhNfTestIconMask(PH_ICON_ALL))
            {
                ShowWindow(PhMainWndHandle, SW_HIDE);
                return TRUE;
            }
        }
        break;
    case SC_MINIMIZE:
        {
            // Save the current window state because we may not have a chance to later.
            PhMwpSaveWindowState();

            if (PhGetIntegerSetting(L"HideOnMinimize") && PhNfTestIconMask(PH_ICON_ALL))
            {
                ShowWindow(PhMainWndHandle, SW_HIDE);
                return TRUE;
            }
        }
        break;
    }

    return FALSE;
}

VOID PhMwpOnMenuCommand(
    _In_ ULONG Index,
    _In_ HMENU Menu
    )
{
    MENUITEMINFO menuItemInfo;

    menuItemInfo.cbSize = sizeof(MENUITEMINFO);
    menuItemInfo.fMask = MIIM_ID | MIIM_DATA;

    if (GetMenuItemInfo(Menu, Index, TRUE, &menuItemInfo))
    {
        PhMwpDispatchMenuCommand(Menu, Index, menuItemInfo.wID, menuItemInfo.dwItemData);
    }
}

VOID PhMwpOnInitMenuPopup(
    _In_ HMENU Menu,
    _In_ ULONG Index,
    _In_ BOOLEAN IsWindowMenu
    )
{
    ULONG i;
    BOOLEAN found;
    MENUINFO menuInfo;
    PPH_EMENU menu;

    found = FALSE;

    for (i = 0; i < sizeof(SubMenuHandles) / sizeof(HWND); i++)
    {
        if (Menu == SubMenuHandles[i])
        {
            found = TRUE;
            break;
        }
    }

    if (!found)
        return;

    // Delete all items in this submenu.
    while (DeleteMenu(Menu, 0, MF_BYPOSITION)) ;

    // Delete the previous EMENU for this submenu.
    if (SubMenuObjects[Index])
        PhDestroyEMenu(SubMenuObjects[Index]);

    // Make sure the menu style is set correctly.
    memset(&menuInfo, 0, sizeof(MENUINFO));
    menuInfo.cbSize = sizeof(MENUINFO);
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_CHECKORBMP;
    SetMenuInfo(Menu, &menuInfo);

    menu = PhCreateEMenu();

    if (Index == 3) // Special case for Users menu.
    {
        PhMwpUpdateUsersMenu(menu);
    }
    else
    {
        PhLoadResourceEMenuItem(menu, PhInstanceHandle, MAKEINTRESOURCE(IDR_MAINWND), Index);
        PhMwpInitializeSubMenu(menu, Index);
    }

    if (PhPluginsEnabled)
    {
        PH_PLUGIN_MENU_INFORMATION menuInfo;

        PhPluginInitializeMenuInfo(&menuInfo, menu, PhMainWndHandle, PH_PLUGIN_MENU_DISALLOW_HOOKS);
        menuInfo.u.MainMenu.SubMenuIndex = Index;
        PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackMainMenuInitializing), &menuInfo);
    }

    PhEMenuToHMenu2(Menu, menu, 0, NULL);
    SubMenuObjects[Index] = menu;
}

VOID PhMwpOnSize(
    VOID
    )
{
    if (!IsIconic(PhMainWndHandle))
    {
        HDWP deferHandle;

        deferHandle = BeginDeferWindowPos(2);
        PhMwpLayout(&deferHandle);
        EndDeferWindowPos(deferHandle);
    }
}

VOID PhMwpOnSizing(
    _In_ ULONG Edge,
    _In_ PRECT DragRectangle
    )
{
    PhResizingMinimumSize(DragRectangle, Edge, 400, 340);
}

VOID PhMwpOnSetFocus(
    VOID
    )
{
    if (CurrentPage->WindowHandle)
        SetFocus(CurrentPage->WindowHandle);
}

VOID PhMwpOnTimer(
    _In_ ULONG Id
    )
{
    if (Id == TIMER_FLUSH_PROCESS_QUERY_DATA)
    {
        static ULONG state = 1;

        // If the update interval is too large, the user might have to wait a while before seeing some types of
        // process-related data. Here we force an update.
        //
        // In addition, we force updates shortly after the program starts up to make things appear more quickly.

        switch (state)
        {
        case 1:
            state = 2;

            if (PhCsUpdateInterval > PH_FLUSH_PROCESS_QUERY_DATA_INTERVAL_2)
                SetTimer(PhMainWndHandle, TIMER_FLUSH_PROCESS_QUERY_DATA, PH_FLUSH_PROCESS_QUERY_DATA_INTERVAL_2, NULL);
            else
                KillTimer(PhMainWndHandle, TIMER_FLUSH_PROCESS_QUERY_DATA);

            break;
        case 2:
            state = 3;

            if (PhCsUpdateInterval > PH_FLUSH_PROCESS_QUERY_DATA_INTERVAL_LONG_TERM)
                SetTimer(PhMainWndHandle, TIMER_FLUSH_PROCESS_QUERY_DATA, PH_FLUSH_PROCESS_QUERY_DATA_INTERVAL_LONG_TERM, NULL);
            else
                KillTimer(PhMainWndHandle, TIMER_FLUSH_PROCESS_QUERY_DATA);

            break;
        default:
            NOTHING;
            break;
        }

        PhFlushProcessQueryData(TRUE);
    }
}

BOOLEAN PhMwpOnNotify(
    _In_ NMHDR *Header,
    _Out_ LRESULT *Result
    )
{
    if (Header->hwndFrom == TabControlHandle)
    {
        PhMwpNotifyTabControl(Header);
    }
    else if (Header->code == RFN_VALIDATE)
    {
        LPNMRUNFILEDLG runFileDlg = (LPNMRUNFILEDLG)Header;

        if (SelectedRunAsMode == RUNAS_MODE_ADMIN)
        {
            PH_STRINGREF string;
            PH_STRINGREF fileName;
            PH_STRINGREF arguments;
            PPH_STRING fullFileName;
            PPH_STRING argumentsString;

            PhInitializeStringRefLongHint(&string, (PWSTR)runFileDlg->lpszFile);
            PhParseCommandLineFuzzy(&string, &fileName, &arguments, &fullFileName);

            if (!fullFileName)
                fullFileName = PhCreateString2(&fileName);

            argumentsString = PhCreateString2(&arguments);

            if (PhShellExecuteEx(PhMainWndHandle, fullFileName->Buffer, argumentsString->Buffer,
                runFileDlg->nShow, PH_SHELL_EXECUTE_ADMIN, 0, NULL))
            {
                *Result = RF_CANCEL;
            }
            else
            {
                *Result = RF_RETRY;
            }

            PhDereferenceObject(fullFileName);
            PhDereferenceObject(argumentsString);

            return TRUE;
        }
        else if (SelectedRunAsMode == RUNAS_MODE_LIMITED)
        {
            NTSTATUS status;
            HANDLE tokenHandle;
            HANDLE newTokenHandle;

            if (NT_SUCCESS(status = PhOpenProcessToken(
                NtCurrentProcess(),
                TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_GROUPS |
                TOKEN_ADJUST_DEFAULT | READ_CONTROL | WRITE_DAC,
                &tokenHandle
                )))
            {
                if (NT_SUCCESS(status = PhFilterTokenForLimitedUser(
                    tokenHandle,
                    &newTokenHandle
                    )))
                {
                    status = PhCreateProcessWin32(
                        NULL,
                        (PWSTR)runFileDlg->lpszFile,
                        NULL,
                        NULL,
                        0,
                        newTokenHandle,
                        NULL,
                        NULL
                        );

                    NtClose(newTokenHandle);
                }

                NtClose(tokenHandle);
            }

            if (NT_SUCCESS(status))
            {
                *Result = RF_CANCEL;
            }
            else
            {
                PhShowStatus(PhMainWndHandle, L"Unable to execute the program.", status, 0);
                *Result = RF_RETRY;
            }

            return TRUE;
        }
    }

    return FALSE;
}

ULONG_PTR PhMwpOnUserMessage(
    _In_ ULONG Message,
    _In_ ULONG_PTR WParam,
    _In_ ULONG_PTR LParam
    )
{
    switch (Message)
    {
    case WM_PH_ACTIVATE:
        {
            if (!PhMainWndExiting)
            {
                if (WParam != 0)
                {
                    PPH_PROCESS_NODE processNode;

                    if (processNode = PhFindProcessNode((HANDLE)WParam))
                        PhSelectAndEnsureVisibleProcessNode(processNode);
                }

                if (!IsWindowVisible(PhMainWndHandle))
                {
                    ShowWindow(PhMainWndHandle, SW_SHOW);
                }

                if (IsIconic(PhMainWndHandle))
                {
                    ShowWindow(PhMainWndHandle, SW_RESTORE);
                }

                return PH_ACTIVATE_REPLY;
            }
            else
            {
                return 0;
            }
        }
        break;
    case WM_PH_SHOW_PROCESS_PROPERTIES:
        {
            PhMwpShowProcessProperties((PPH_PROCESS_ITEM)LParam);
        }
        break;
    case WM_PH_DESTROY:
        {
            DestroyWindow(PhMainWndHandle);
        }
        break;
    case WM_PH_SAVE_ALL_SETTINGS:
        {
            PhMwpSaveSettings();
        }
        break;
    case WM_PH_PREPARE_FOR_EARLY_SHUTDOWN:
        {
            PhMwpSaveSettings();
            PhMainWndExiting = TRUE;
        }
        break;
    case WM_PH_CANCEL_EARLY_SHUTDOWN:
        {
            PhMainWndExiting = FALSE;
        }
        break;
    case WM_PH_DELAYED_LOAD_COMPLETED:
        {
            // Nothing
        }
        break;
    case WM_PH_NOTIFY_ICON_MESSAGE:
        {
            PhNfForwardMessage(WParam, LParam);
        }
        break;
    case WM_PH_TOGGLE_VISIBLE:
        {
            PhMwpActivateWindow(!WParam);
        }
        break;
    case WM_PH_SHOW_MEMORY_EDITOR:
        {
            PPH_SHOW_MEMORY_EDITOR showMemoryEditor = (PPH_SHOW_MEMORY_EDITOR)LParam;

            PhShowMemoryEditorDialog(
                showMemoryEditor->OwnerWindow,
                showMemoryEditor->ProcessId,
                showMemoryEditor->BaseAddress,
                showMemoryEditor->RegionSize,
                showMemoryEditor->SelectOffset,
                showMemoryEditor->SelectLength,
                showMemoryEditor->Title,
                showMemoryEditor->Flags
                );
            PhClearReference(&showMemoryEditor->Title);
            PhFree(showMemoryEditor);
        }
        break;
    case WM_PH_SHOW_MEMORY_RESULTS:
        {
            PPH_SHOW_MEMORY_RESULTS showMemoryResults = (PPH_SHOW_MEMORY_RESULTS)LParam;

            PhShowMemoryResultsDialog(
                showMemoryResults->ProcessId,
                showMemoryResults->Results
                );
            PhDereferenceMemoryResults(
                (PPH_MEMORY_RESULT *)showMemoryResults->Results->Items,
                showMemoryResults->Results->Count
                );
            PhDereferenceObject(showMemoryResults->Results);
            PhFree(showMemoryResults);
        }
        break;
    case WM_PH_SELECT_TAB_PAGE:
        {
            ULONG index = (ULONG)WParam;

            PhMwpSelectPage(index);

            if (CurrentPage->WindowHandle)
                SetFocus(CurrentPage->WindowHandle);
        }
        break;
    case WM_PH_GET_CALLBACK_LAYOUT_PADDING:
        {
            return (ULONG_PTR)&LayoutPaddingCallback;
        }
        break;
    case WM_PH_INVALIDATE_LAYOUT_PADDING:
        {
            LayoutPaddingValid = FALSE;
        }
        break;
    case WM_PH_SELECT_PROCESS_NODE:
        {
            PhSelectAndEnsureVisibleProcessNode((PPH_PROCESS_NODE)LParam);
        }
        break;
    case WM_PH_SELECT_SERVICE_ITEM:
        {
            PPH_SERVICE_NODE serviceNode;

            PhMwpNeedServiceTreeList();

            // For compatibility, LParam is a service item, not node.
            if (serviceNode = PhFindServiceNode((PPH_SERVICE_ITEM)LParam))
            {
                PhSelectAndEnsureVisibleServiceNode(serviceNode);
            }
        }
        break;
    case WM_PH_SELECT_NETWORK_ITEM:
        {
            PPH_NETWORK_NODE networkNode;

            PhMwpNeedNetworkTreeList();

            // For compatibility, LParam is a network item, not node.
            if (networkNode = PhFindNetworkNode((PPH_NETWORK_ITEM)LParam))
            {
                PhSelectAndEnsureVisibleNetworkNode(networkNode);
            }
        }
        break;
    case WM_PH_UPDATE_FONT:
        {
            PPH_STRING fontHexString;
            LOGFONT font;

            fontHexString = PhaGetStringSetting(L"Font");

            if (
                fontHexString->Length / 2 / 2 == sizeof(LOGFONT) &&
                PhHexStringToBuffer(&fontHexString->sr, (PUCHAR)&font)
                )
            {
                HFONT newFont;

                newFont = CreateFontIndirect(&font);

                if (newFont)
                {
                    if (CurrentCustomFont)
                        DeleteObject(CurrentCustomFont);
                    CurrentCustomFont = newFont;

                    PhMwpNotifyAllPages(MainTabPageFontChanged, newFont, NULL);
                }
            }
        }
        break;
    case WM_PH_GET_FONT:
        return SendMessage(PhMwpProcessTreeNewHandle, WM_GETFONT, 0, 0);
    case WM_PH_INVOKE:
        {
            VOID (NTAPI *function)(PVOID);

            function = (PVOID)LParam;
            function((PVOID)WParam);
        }
        break;
    case WM_PH_CREATE_TAB_PAGE:
        {
            return (ULONG_PTR)PhMwpCreatePage((PPH_MAIN_TAB_PAGE)LParam);
        }
        break;
    case WM_PH_REFRESH:
        {
            SendMessage(PhMainWndHandle, WM_COMMAND, ID_VIEW_REFRESH, 0);
        }
        break;
    case WM_PH_GET_UPDATE_AUTOMATICALLY:
        {
            return PhMwpUpdateAutomatically;
        }
        break;
    case WM_PH_SET_UPDATE_AUTOMATICALLY:
        {
            if (!!WParam != PhMwpUpdateAutomatically)
            {
                SendMessage(PhMainWndHandle, WM_COMMAND, ID_VIEW_UPDATEAUTOMATICALLY, 0);
            }
        }
        break;
    case WM_PH_ICON_CLICK:
        {
            PhMwpActivateWindow(!!PhGetIntegerSetting(L"IconTogglesVisibility"));
        }
        break;
    case WM_PH_PROCESS_ADDED:
        {
            ULONG runId = (ULONG)WParam;
            PPH_PROCESS_ITEM processItem = (PPH_PROCESS_ITEM)LParam;

            PhMwpOnProcessAdded(processItem, runId);
        }
        break;
    case WM_PH_PROCESS_MODIFIED:
        {
            PhMwpOnProcessModified((PPH_PROCESS_ITEM)LParam);
        }
        break;
    case WM_PH_PROCESS_REMOVED:
        {
            PhMwpOnProcessRemoved((PPH_PROCESS_ITEM)LParam);
        }
        break;
    case WM_PH_PROCESSES_UPDATED:
        {
            PhMwpOnProcessesUpdated();
        }
        break;
    case WM_PH_SERVICE_ADDED:
        {
            ULONG runId = (ULONG)WParam;
            PPH_SERVICE_ITEM serviceItem = (PPH_SERVICE_ITEM)LParam;

            PhMwpOnServiceAdded(serviceItem, runId);
        }
        break;
    case WM_PH_SERVICE_MODIFIED:
        {
            PPH_SERVICE_MODIFIED_DATA serviceModifiedData = (PPH_SERVICE_MODIFIED_DATA)LParam;

            PhMwpOnServiceModified(serviceModifiedData);
            PhFree(serviceModifiedData);
        }
        break;
    case WM_PH_SERVICE_REMOVED:
        {
            PhMwpOnServiceRemoved((PPH_SERVICE_ITEM)LParam);
        }
        break;
    case WM_PH_SERVICES_UPDATED:
        {
            PhMwpOnServicesUpdated();
        }
        break;
    case WM_PH_NETWORK_ITEM_ADDED:
        {
            ULONG runId = (ULONG)WParam;
            PPH_NETWORK_ITEM networkItem = (PPH_NETWORK_ITEM)LParam;

            PhMwpOnNetworkItemAdded(runId, networkItem);
        }
        break;
    case WM_PH_NETWORK_ITEM_MODIFIED:
        {
            PhMwpOnNetworkItemModified((PPH_NETWORK_ITEM)LParam);
        }
        break;
    case WM_PH_NETWORK_ITEM_REMOVED:
        {
            PhMwpOnNetworkItemRemoved((PPH_NETWORK_ITEM)LParam);
        }
        break;
    case WM_PH_NETWORK_ITEMS_UPDATED:
        {
            PhMwpOnNetworkItemsUpdated();
        }
        break;
    }

    return 0;
}

VOID NTAPI PhMwpNetworkItemAddedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_NETWORK_ITEM networkItem = (PPH_NETWORK_ITEM)Parameter;

    PhReferenceObject(networkItem);
    PostMessage(
        PhMainWndHandle,
        WM_PH_NETWORK_ITEM_ADDED,
        PhGetRunIdProvider(&PhMwpNetworkProviderRegistration),
        (LPARAM)networkItem
        );
}

VOID NTAPI PhMwpNetworkItemModifiedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_NETWORK_ITEM networkItem = (PPH_NETWORK_ITEM)Parameter;

    PostMessage(PhMainWndHandle, WM_PH_NETWORK_ITEM_MODIFIED, 0, (LPARAM)networkItem);
}

VOID NTAPI PhMwpNetworkItemRemovedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_NETWORK_ITEM networkItem = (PPH_NETWORK_ITEM)Parameter;

    PostMessage(PhMainWndHandle, WM_PH_NETWORK_ITEM_REMOVED, 0, (LPARAM)networkItem);
}

VOID NTAPI PhMwpNetworkItemsUpdatedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PostMessage(PhMainWndHandle, WM_PH_NETWORK_ITEMS_UPDATED, 0, 0);
}

VOID PhMwpLoadSettings(
    VOID
    )
{
    ULONG opacity;
    PPH_STRING customFont;

    if (PhGetIntegerSetting(L"MainWindowAlwaysOnTop"))
    {
        AlwaysOnTop = TRUE;
        SetWindowPos(PhMainWndHandle, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOSIZE);
    }

    opacity = PhGetIntegerSetting(L"MainWindowOpacity");

    if (opacity != 0)
        PhSetWindowOpacity(PhMainWndHandle, opacity);

    PhStatisticsSampleCount = PhGetIntegerSetting(L"SampleCount");
    PhEnablePurgeProcessRecords = !PhGetIntegerSetting(L"NoPurgeProcessRecords");
    PhEnableCycleCpuUsage = !!PhGetIntegerSetting(L"EnableCycleCpuUsage");
    PhEnableServiceNonPoll = !!PhGetIntegerSetting(L"EnableServiceNonPoll");
    PhEnableNetworkProviderResolve = !!PhGetIntegerSetting(L"EnableNetworkResolve");

    PhNfLoadStage1();
    PhMwpNotifyIconNotifyMask = PhGetIntegerSetting(L"IconNotifyMask");

    customFont = PhaGetStringSetting(L"Font");

    if (customFont->Length / 2 / 2 == sizeof(LOGFONT))
        SendMessage(PhMainWndHandle, WM_PH_UPDATE_FONT, 0, 0);

    PhMwpNotifyAllPages(MainTabPageLoadSettings, NULL, NULL);
}

VOID PhMwpSaveSettings(
    VOID
    )
{
    PhMwpNotifyAllPages(MainTabPageSaveSettings, NULL, NULL);

    PhNfSaveSettings();
    PhSetIntegerSetting(L"IconNotifyMask", PhMwpNotifyIconNotifyMask);

    PhSaveWindowPlacementToSetting(L"MainWindowPosition", L"MainWindowSize", PhMainWndHandle);
    PhMwpSaveWindowState();

    if (PhSettingsFileName)
        PhSaveSettings(PhSettingsFileName->Buffer);
}

VOID PhMwpSaveWindowState(
    VOID
    )
{
    WINDOWPLACEMENT placement = { sizeof(placement) };

    GetWindowPlacement(PhMainWndHandle, &placement);

    if (placement.showCmd == SW_NORMAL)
        PhSetIntegerSetting(L"MainWindowState", SW_NORMAL);
    else if (placement.showCmd == SW_MAXIMIZE)
        PhSetIntegerSetting(L"MainWindowState", SW_MAXIMIZE);
}

VOID PhMwpSymInitHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_STRING dbghelpPath;

    dbghelpPath = PhGetStringSetting(L"DbgHelpPath");
    PhLoadSymbolProviderDbgHelpFromPath(dbghelpPath->Buffer);
    PhDereferenceObject(dbghelpPath);
}

VOID PhMwpUpdateLayoutPadding(
    VOID
    )
{
    PH_LAYOUT_PADDING_DATA data;

    memset(&data, 0, sizeof(PH_LAYOUT_PADDING_DATA));
    PhInvokeCallback(&LayoutPaddingCallback, &data);

    LayoutPadding = data.Padding;
}

VOID PhMwpApplyLayoutPadding(
    _Inout_ PRECT Rect,
    _In_ PRECT Padding
    )
{
    Rect->left += Padding->left;
    Rect->top += Padding->top;
    Rect->right -= Padding->right;
    Rect->bottom -= Padding->bottom;
}

VOID PhMwpLayout(
    _Inout_ HDWP *DeferHandle
    )
{
    RECT rect;

    // Resize the tab control.
    // Don't defer the resize. The tab control doesn't repaint properly.

    if (!LayoutPaddingValid)
    {
        PhMwpUpdateLayoutPadding();
        LayoutPaddingValid = TRUE;
    }

    GetClientRect(PhMainWndHandle, &rect);
    PhMwpApplyLayoutPadding(&rect, &LayoutPadding);

    SetWindowPos(TabControlHandle, NULL,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOACTIVATE | SWP_NOZORDER);
    UpdateWindow(TabControlHandle);

    PhMwpLayoutTabControl(DeferHandle);
}

VOID PhMwpSetupComputerMenu(
    _In_ PPH_EMENU_ITEM Root
    )
{
    PPH_EMENU_ITEM menuItem;

    if (WindowsVersion < WINDOWS_8)
    {
        if (menuItem = PhFindEMenuItem(Root, PH_EMENU_FIND_DESCEND, NULL, ID_COMPUTER_RESTARTBOOTOPTIONS))
            PhDestroyEMenuItem(menuItem);
        if (menuItem = PhFindEMenuItem(Root, PH_EMENU_FIND_DESCEND, NULL, ID_COMPUTER_SHUTDOWNHYBRID))
            PhDestroyEMenuItem(menuItem);
    }
}

BOOLEAN PhMwpExecuteComputerCommand(
    _In_ ULONG Id
    )
{
    switch (Id)
    {
    case ID_COMPUTER_LOCK:
        PhUiLockComputer(PhMainWndHandle);
        return TRUE;
    case ID_COMPUTER_LOGOFF:
        PhUiLogoffComputer(PhMainWndHandle);
        return TRUE;
    case ID_COMPUTER_SLEEP:
        PhUiSleepComputer(PhMainWndHandle);
        return TRUE;
    case ID_COMPUTER_HIBERNATE:
        PhUiHibernateComputer(PhMainWndHandle);
        return TRUE;
    case ID_COMPUTER_RESTART:
        PhUiRestartComputer(PhMainWndHandle, 0);
        return TRUE;
    case ID_COMPUTER_RESTARTBOOTOPTIONS:
        PhUiRestartComputer(PhMainWndHandle, EWX_BOOTOPTIONS);
        return TRUE;
    case ID_COMPUTER_SHUTDOWN:
        PhUiShutdownComputer(PhMainWndHandle, 0);
        return TRUE;
    case ID_COMPUTER_SHUTDOWNHYBRID:
        PhUiShutdownComputer(PhMainWndHandle, EWX_HYBRID_SHUTDOWN);
        return TRUE;
    }

    return FALSE;
}

VOID PhMwpActivateWindow(
    _In_ BOOLEAN Toggle
    )
{
    if (IsIconic(PhMainWndHandle))
    {
        ShowWindow(PhMainWndHandle, SW_RESTORE);
        SetForegroundWindow(PhMainWndHandle);
    }
    else if (IsWindowVisible(PhMainWndHandle))
    {
        if (Toggle)
            ShowWindow(PhMainWndHandle, SW_HIDE);
        else
            SetForegroundWindow(PhMainWndHandle);
    }
    else
    {
        ShowWindow(PhMainWndHandle, SW_SHOW);
        SetForegroundWindow(PhMainWndHandle);
    }
}

VOID PhMwpInitializeMainMenu(
    _In_ HMENU Menu
    )
{
    MENUINFO menuInfo;
    ULONG i;

    menuInfo.cbSize = sizeof(MENUINFO);
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_NOTIFYBYPOS;
    SetMenuInfo(Menu, &menuInfo);

    for (i = 0; i < sizeof(SubMenuHandles) / sizeof(HMENU); i++)
    {
        SubMenuHandles[i] = GetSubMenu(PhMainWndMenuHandle, i);
    }
}

VOID PhMwpDispatchMenuCommand(
    _In_ HMENU MenuHandle,
    _In_ ULONG ItemIndex,
    _In_ ULONG ItemId,
    _In_ ULONG_PTR ItemData
    )
{
    switch (ItemId)
    {
    case ID_PLUGIN_MENU_ITEM:
        {
            PPH_EMENU_ITEM menuItem;
            PH_PLUGIN_MENU_INFORMATION menuInfo;

            menuItem = (PPH_EMENU_ITEM)ItemData;

            if (menuItem)
            {
                PhPluginInitializeMenuInfo(&menuInfo, NULL, PhMainWndHandle, 0);
                PhPluginTriggerEMenuItem(&menuInfo, menuItem);
            }

            return;
        }
        break;
    case ID_TRAYICONS_REGISTERED:
        {
            PPH_EMENU_ITEM menuItem;

            menuItem = (PPH_EMENU_ITEM)ItemData;

            if (menuItem)
            {
                PPH_NF_ICON icon;

                icon = menuItem->Context;
                PhNfSetVisibleIcon(icon->IconId, !PhNfTestIconMask(icon->IconId));
            }

            return;
        }
        break;
    case ID_USER_CONNECT:
    case ID_USER_DISCONNECT:
    case ID_USER_LOGOFF:
    case ID_USER_REMOTECONTROL:
    case ID_USER_SENDMESSAGE:
    case ID_USER_PROPERTIES:
        {
            PPH_EMENU_ITEM menuItem;

            menuItem = (PPH_EMENU_ITEM)ItemData;

            if (menuItem && menuItem->Parent)
            {
                SelectedUserSessionId = PtrToUlong(menuItem->Parent->Context);
            }
        }
        break;
    }

    SendMessage(PhMainWndHandle, WM_COMMAND, ItemId, 0);
}

HBITMAP PhMwpGetShieldBitmap(
    VOID
    )
{
    static HBITMAP shieldBitmap = NULL;

    if (!shieldBitmap)
    {
        HICON shieldIcon;

        if (shieldIcon = PhLoadIcon(NULL, IDI_SHIELD, PH_LOAD_ICON_SIZE_SMALL | PH_LOAD_ICON_STRICT, 0, 0))
        {
            shieldBitmap = PhIconToBitmap(shieldIcon, PhSmallIconSize.X, PhSmallIconSize.Y);
            DestroyIcon(shieldIcon);
        }
    }

    return shieldBitmap;
}

VOID PhMwpInitializeSubMenu(
    _In_ PPH_EMENU Menu,
    _In_ ULONG Index
    )
{
    PPH_EMENU_ITEM menuItem;

    if (Index == 0) // Hacker
    {
        // Fix some menu items.
        if (PhGetOwnTokenAttributes().Elevated)
        {
            if (menuItem = PhFindEMenuItem(Menu, 0, NULL, ID_HACKER_RUNASADMINISTRATOR))
                PhDestroyEMenuItem(menuItem);
            if (menuItem = PhFindEMenuItem(Menu, 0, NULL, ID_HACKER_SHOWDETAILSFORALLPROCESSES))
                PhDestroyEMenuItem(menuItem);
        }
        else
        {
            HBITMAP shieldBitmap;

            if (shieldBitmap = PhMwpGetShieldBitmap())
            {
                if (menuItem = PhFindEMenuItem(Menu, 0, NULL, ID_HACKER_SHOWDETAILSFORALLPROCESSES))
                    menuItem->Bitmap = shieldBitmap;
            }
        }

        // Fix up the Computer menu.
        PhMwpSetupComputerMenu(Menu);
    }
    else if (Index == 1) // View
    {
        PPH_EMENU_ITEM trayIconsMenuItem;
        ULONG i;
        PPH_EMENU_ITEM menuItem;
        ULONG id;
        ULONG placeholderIndex;

        trayIconsMenuItem = PhMwpFindTrayIconsMenuItem(Menu);

        if (trayIconsMenuItem)
        {
            ULONG maximum;
            PPH_NF_ICON icon;

            // Add menu items for the registered tray icons.

            id = PH_ICON_DEFAULT_MAXIMUM;
            maximum = PhNfGetMaximumIconId();

            for (; id != maximum; id <<= 1)
            {
                if (icon = PhNfGetIconById(id))
                {
                    PhInsertEMenuItem(trayIconsMenuItem, PhCreateEMenuItem(0, ID_TRAYICONS_REGISTERED, icon->Text, NULL, icon), -1);
                }
            }

            // Update the text and check marks on the menu items.

            for (i = 0; i < trayIconsMenuItem->Items->Count; i++)
            {
                menuItem = trayIconsMenuItem->Items->Items[i];

                id = -1;
                icon = NULL;

                switch (menuItem->Id)
                {
                case ID_TRAYICONS_CPUHISTORY:
                    id = PH_ICON_CPU_HISTORY;
                    break;
                case ID_TRAYICONS_IOHISTORY:
                    id = PH_ICON_IO_HISTORY;
                    break;
                case ID_TRAYICONS_COMMITHISTORY:
                    id = PH_ICON_COMMIT_HISTORY;
                    break;
                case ID_TRAYICONS_PHYSICALMEMORYHISTORY:
                    id = PH_ICON_PHYSICAL_HISTORY;
                    break;
                case ID_TRAYICONS_CPUUSAGE:
                    id = PH_ICON_CPU_USAGE;
                    break;
                case ID_TRAYICONS_REGISTERED:
                    icon = menuItem->Context;
                    id = icon->IconId;
                    break;
                }

                if (id != -1)
                {
                    if (PhNfTestIconMask(id))
                        menuItem->Flags |= PH_EMENU_CHECKED;

                    if (icon && (icon->Flags & PH_NF_ICON_UNAVAILABLE))
                    {
                        PPH_STRING newText;

                        newText = PhaConcatStrings2(icon->Text, L" (Unavailable)");
                        PhModifyEMenuItem(menuItem, PH_EMENU_MODIFY_TEXT, PH_EMENU_TEXT_OWNED,
                            PhAllocateCopy(newText->Buffer, newText->Length + sizeof(WCHAR)), NULL);
                    }
                }
            }
        }

        if (menuItem = PhFindEMenuItemEx(Menu, 0, NULL, ID_VIEW_SECTIONPLACEHOLDER, NULL, &placeholderIndex))
        {
            PhDestroyEMenuItem(menuItem);
            PhMwpInitializeSectionMenuItems(Menu, placeholderIndex);
        }

        if (AlwaysOnTop && (menuItem = PhFindEMenuItem(Menu, 0, NULL, ID_VIEW_ALWAYSONTOP)))
            menuItem->Flags |= PH_EMENU_CHECKED;

        id = PH_OPACITY_TO_ID(PhGetIntegerSetting(L"MainWindowOpacity"));

        if (menuItem = PhFindEMenuItem(Menu, PH_EMENU_FIND_DESCEND, NULL, id))
            menuItem->Flags |= PH_EMENU_CHECKED | PH_EMENU_RADIOCHECK;

        switch (PhGetIntegerSetting(L"UpdateInterval"))
        {
        case 500:
            id = ID_UPDATEINTERVAL_FAST;
            break;
        case 1000:
            id = ID_UPDATEINTERVAL_NORMAL;
            break;
        case 2000:
            id = ID_UPDATEINTERVAL_BELOWNORMAL;
            break;
        case 5000:
            id = ID_UPDATEINTERVAL_SLOW;
            break;
        case 10000:
            id = ID_UPDATEINTERVAL_VERYSLOW;
            break;
        default:
            id = -1;
            break;
        }

        if (id != -1 && (menuItem = PhFindEMenuItem(Menu, PH_EMENU_FIND_DESCEND, NULL, id)))
            menuItem->Flags |= PH_EMENU_CHECKED | PH_EMENU_RADIOCHECK;

        if (PhMwpUpdateAutomatically && (menuItem = PhFindEMenuItem(Menu, 0, NULL, ID_VIEW_UPDATEAUTOMATICALLY)))
            menuItem->Flags |= PH_EMENU_CHECKED;
    }
    else if (Index == 2) // Tools
    {
        if (!PhGetIntegerSetting(L"HiddenProcessesMenuEnabled"))
        {
            if (menuItem = PhFindEMenuItem(Menu, 0, NULL, ID_TOOLS_HIDDENPROCESSES))
                PhDestroyEMenuItem(menuItem);
        }

        // Windows 8 Task Manager requires elevation.
        if (WindowsVersion >= WINDOWS_8 && !PhGetOwnTokenAttributes().Elevated)
        {
            HBITMAP shieldBitmap;

            if (shieldBitmap = PhMwpGetShieldBitmap())
            {
                if (menuItem = PhFindEMenuItem(Menu, 0, NULL, ID_TOOLS_STARTTASKMANAGER))
                    menuItem->Bitmap = shieldBitmap;
            }
        }
    }
}

PPH_EMENU_ITEM PhMwpFindTrayIconsMenuItem(
    _In_ PPH_EMENU Menu
    )
{
    ULONG i;
    PPH_EMENU_ITEM menuItem;

    for (i = 0; i < Menu->Items->Count; i++)
    {
        menuItem = Menu->Items->Items[i];

        if (PhFindEMenuItem(menuItem, 0, NULL, ID_TRAYICONS_CPUHISTORY))
            return menuItem;
    }

    return NULL;
}

VOID PhMwpInitializeSectionMenuItems(
    _In_ PPH_EMENU Menu,
    _In_ ULONG StartIndex
    )
{
    if (CurrentPage)
    {
        PH_MAIN_TAB_PAGE_MENU_INFORMATION menuInfo;

        menuInfo.Menu = Menu;
        menuInfo.StartIndex = StartIndex;

        if (!CurrentPage->Callback(CurrentPage, MainTabPageInitializeSectionMenuItems, &menuInfo, NULL))
        {
            // Remove the extra separator.
            PhRemoveEMenuItem(Menu, NULL, StartIndex);
        }
    }
}

VOID PhMwpLayoutTabControl(
    _Inout_ HDWP *DeferHandle
    )
{
    RECT rect;

    if (!LayoutPaddingValid)
    {
        PhMwpUpdateLayoutPadding();
        LayoutPaddingValid = TRUE;
    }

    GetClientRect(PhMainWndHandle, &rect);
    PhMwpApplyLayoutPadding(&rect, &LayoutPadding);
    TabCtrl_AdjustRect(TabControlHandle, FALSE, &rect);

    if (CurrentPage && CurrentPage->WindowHandle)
    {
        *DeferHandle = DeferWindowPos(*DeferHandle, CurrentPage->WindowHandle, NULL,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOACTIVATE | SWP_NOZORDER);
    }
}

VOID PhMwpNotifyTabControl(
    _In_ NMHDR *Header
    )
{
    if (Header->code == TCN_SELCHANGING)
    {
        OldTabIndex = TabCtrl_GetCurSel(TabControlHandle);
    }
    else if (Header->code == TCN_SELCHANGE)
    {
        PhMwpSelectionChangedTabControl(OldTabIndex);
    }
}

VOID PhMwpSelectionChangedTabControl(
    _In_ ULONG OldIndex
    )
{
    ULONG selectedIndex;
    HDWP deferHandle;
    ULONG i;

    selectedIndex = TabCtrl_GetCurSel(TabControlHandle);

    if (selectedIndex == OldIndex)
        return;

    deferHandle = BeginDeferWindowPos(3);

    for (i = 0; i < PageList->Count; i++)
    {
        PPH_MAIN_TAB_PAGE page = PageList->Items[i];

        page->Selected = page->Index == selectedIndex;

        if (page->Index == selectedIndex)
        {
            CurrentPage = page;

            // Create the tab page window if it doesn't exist.
            if (!page->WindowHandle && !page->CreateWindowCalled)
            {
                if (page->Callback(page, MainTabPageCreateWindow, &page->WindowHandle, NULL))
                    page->CreateWindowCalled = TRUE;

                if (page->WindowHandle)
                    BringWindowToTop(page->WindowHandle);
                if (CurrentCustomFont)
                    page->Callback(page, MainTabPageFontChanged, CurrentCustomFont, NULL);
            }

            page->Callback(page, MainTabPageSelected, (PVOID)TRUE, NULL);

            if (page->WindowHandle)
            {
                deferHandle = DeferWindowPos(deferHandle, page->WindowHandle, NULL, 0, 0, 0, 0, SWP_SHOWWINDOW_ONLY);
                SetFocus(page->WindowHandle);
            }
        }
        else if (page->Index == OldIndex)
        {
            page->Callback(page, MainTabPageSelected, (PVOID)FALSE, NULL);

            if (page->WindowHandle)
            {
                deferHandle = DeferWindowPos(deferHandle, page->WindowHandle, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW_ONLY);
            }
        }
    }

    PhMwpLayoutTabControl(&deferHandle);

    EndDeferWindowPos(deferHandle);

    if (PhPluginsEnabled)
        PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackMainWindowTabChanged), IntToPtr(selectedIndex));
}

PPH_MAIN_TAB_PAGE PhMwpCreatePage(
    _In_ PPH_MAIN_TAB_PAGE Template
    )
{
    PPH_MAIN_TAB_PAGE page;
    PPH_STRING name;
    HDWP deferHandle;

    page = PhAllocate(sizeof(PH_MAIN_TAB_PAGE));
    memset(page, 0, sizeof(PH_MAIN_TAB_PAGE));

    page->Name = Template->Name;
    page->Flags = Template->Flags;
    page->Callback = Template->Callback;
    page->Context = Template->Context;

    PhAddItemList(PageList, page);

    name = PhCreateString2(&page->Name);
    page->Index = PhAddTabControlTab(TabControlHandle, MAXINT, name->Buffer);
    PhDereferenceObject(name);

    page->Callback(page, MainTabPageCreate, NULL, NULL);

    // The tab control might need multiple lines, so we need to refresh the layout.
    deferHandle = BeginDeferWindowPos(1);
    PhMwpLayoutTabControl(&deferHandle);
    EndDeferWindowPos(deferHandle);

    return page;
}

VOID PhMwpSelectPage(
    _In_ ULONG Index
    )
{
    INT oldIndex;

    oldIndex = TabCtrl_GetCurSel(TabControlHandle);
    TabCtrl_SetCurSel(TabControlHandle, Index);
    PhMwpSelectionChangedTabControl(oldIndex);
}

PPH_MAIN_TAB_PAGE PhMwpFindPage(
    _In_ PPH_STRINGREF Name
    )
{
    ULONG i;

    for (i = 0; i < PageList->Count; i++)
    {
        PPH_MAIN_TAB_PAGE page = PageList->Items[i];

        if (PhEqualStringRef(&page->Name, Name, TRUE))
            return page;
    }

    return NULL;
}

PPH_MAIN_TAB_PAGE PhMwpCreateInternalPage(
    _In_ PWSTR Name,
    _In_ ULONG Flags,
    _In_ PPH_MAIN_TAB_PAGE_CALLBACK Callback
    )
{
    PH_MAIN_TAB_PAGE page;

    memset(&page, 0, sizeof(PH_MAIN_TAB_PAGE));
    PhInitializeStringRef(&page.Name, Name);
    page.Flags = Flags;
    page.Callback = Callback;

    return PhMwpCreatePage(&page);
}

VOID PhMwpNotifyAllPages(
    _In_ PH_MAIN_TAB_PAGE_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2
    )
{
    ULONG i;
    PPH_MAIN_TAB_PAGE page;

    for (i = 0; i < PageList->Count; i++)
    {
        page = PageList->Items[i];
        page->Callback(page, Message, Parameter1, Parameter2);
    }
}

static int __cdecl IconProcessesCpuUsageCompare(
    _In_ const void *elem1,
    _In_ const void *elem2
    )
{
    PPH_PROCESS_ITEM processItem1 = *(PPH_PROCESS_ITEM *)elem1;
    PPH_PROCESS_ITEM processItem2 = *(PPH_PROCESS_ITEM *)elem2;

    return -singlecmp(processItem1->CpuUsage, processItem2->CpuUsage);
}

static int __cdecl IconProcessesNameCompare(
    _In_ const void *elem1,
    _In_ const void *elem2
    )
{
    PPH_PROCESS_ITEM processItem1 = *(PPH_PROCESS_ITEM *)elem1;
    PPH_PROCESS_ITEM processItem2 = *(PPH_PROCESS_ITEM *)elem2;

    return PhCompareString(processItem1->ProcessName, processItem2->ProcessName, TRUE);
}

VOID PhAddMiniProcessMenuItems(
    _Inout_ struct _PH_EMENU_ITEM *Menu,
    _In_ HANDLE ProcessId
    )
{
    PPH_EMENU_ITEM priorityMenu;
    PPH_EMENU_ITEM ioPriorityMenu = NULL;
    PPH_PROCESS_ITEM processItem;
    BOOLEAN isSuspended = FALSE;
    BOOLEAN isPartiallySuspended = TRUE;

    // Priority

    priorityMenu = PhCreateEMenuItem(0, 0, L"Priority", NULL, ProcessId);

    PhInsertEMenuItem(priorityMenu, PhCreateEMenuItem(0, ID_PRIORITY_REALTIME, L"Real time", NULL, ProcessId), -1);
    PhInsertEMenuItem(priorityMenu, PhCreateEMenuItem(0, ID_PRIORITY_HIGH, L"High", NULL, ProcessId), -1);
    PhInsertEMenuItem(priorityMenu, PhCreateEMenuItem(0, ID_PRIORITY_ABOVENORMAL, L"Above normal", NULL, ProcessId), -1);
    PhInsertEMenuItem(priorityMenu, PhCreateEMenuItem(0, ID_PRIORITY_NORMAL, L"Normal", NULL, ProcessId), -1);
    PhInsertEMenuItem(priorityMenu, PhCreateEMenuItem(0, ID_PRIORITY_BELOWNORMAL, L"Below normal", NULL, ProcessId), -1);
    PhInsertEMenuItem(priorityMenu, PhCreateEMenuItem(0, ID_PRIORITY_IDLE, L"Idle", NULL, ProcessId), -1);

    // I/O priority

    ioPriorityMenu = PhCreateEMenuItem(0, 0, L"I/O priority", NULL, ProcessId);

    PhInsertEMenuItem(ioPriorityMenu, PhCreateEMenuItem(0, ID_IOPRIORITY_HIGH, L"High", NULL, ProcessId), -1);
    PhInsertEMenuItem(ioPriorityMenu, PhCreateEMenuItem(0, ID_IOPRIORITY_NORMAL, L"Normal", NULL, ProcessId), -1);
    PhInsertEMenuItem(ioPriorityMenu, PhCreateEMenuItem(0, ID_IOPRIORITY_LOW, L"Low", NULL, ProcessId), -1);
    PhInsertEMenuItem(ioPriorityMenu, PhCreateEMenuItem(0, ID_IOPRIORITY_VERYLOW, L"Very low", NULL, ProcessId), -1);

    // Menu

    PhInsertEMenuItem(Menu, PhCreateEMenuItem(0, ID_PROCESS_TERMINATE, L"T&erminate", NULL, ProcessId), -1);

    if (processItem = PhReferenceProcessItem(ProcessId))
    {
        isSuspended = (BOOLEAN)processItem->IsSuspended;
        isPartiallySuspended = (BOOLEAN)processItem->IsPartiallySuspended;
        PhDereferenceObject(processItem);
    }

    if (!isSuspended)
        PhInsertEMenuItem(Menu, PhCreateEMenuItem(0, ID_PROCESS_SUSPEND, L"&Suspend", NULL, ProcessId), -1);
    if (isPartiallySuspended)
        PhInsertEMenuItem(Menu, PhCreateEMenuItem(0, ID_PROCESS_RESUME, L"Res&ume", NULL, ProcessId), -1);

    PhInsertEMenuItem(Menu, priorityMenu, -1);

    if (ioPriorityMenu)
        PhInsertEMenuItem(Menu, ioPriorityMenu, -1);

    PhMwpSetProcessMenuPriorityChecks(Menu, ProcessId, TRUE, TRUE, FALSE);

    PhInsertEMenuItem(Menu, PhCreateEMenuItem(0, ID_PROCESS_PROPERTIES, L"P&roperties", NULL, ProcessId), -1);
}

BOOLEAN PhHandleMiniProcessMenuItem(
    _Inout_ struct _PH_EMENU_ITEM *MenuItem
    )
{
    switch (MenuItem->Id)
    {
    case ID_PROCESS_TERMINATE:
    case ID_PROCESS_SUSPEND:
    case ID_PROCESS_RESUME:
    case ID_PROCESS_PROPERTIES:
        {
            HANDLE processId = MenuItem->Context;
            PPH_PROCESS_ITEM processItem;

            if (processItem = PhReferenceProcessItem(processId))
            {
                switch (MenuItem->Id)
                {
                case ID_PROCESS_TERMINATE:
                    PhUiTerminateProcesses(PhMainWndHandle, &processItem, 1);
                    break;
                case ID_PROCESS_SUSPEND:
                    PhUiSuspendProcesses(PhMainWndHandle, &processItem, 1);
                    break;
                case ID_PROCESS_RESUME:
                    PhUiResumeProcesses(PhMainWndHandle, &processItem, 1);
                    break;
                case ID_PROCESS_PROPERTIES:
                    ProcessHacker_ShowProcessProperties(PhMainWndHandle, processItem);
                    break;
                }

                PhDereferenceObject(processItem);
            }
            else
            {
                PhShowError(PhMainWndHandle, L"The process does not exist.");
            }
        }
        break;
    case ID_PRIORITY_REALTIME:
    case ID_PRIORITY_HIGH:
    case ID_PRIORITY_ABOVENORMAL:
    case ID_PRIORITY_NORMAL:
    case ID_PRIORITY_BELOWNORMAL:
    case ID_PRIORITY_IDLE:
        {
            HANDLE processId = MenuItem->Context;
            PPH_PROCESS_ITEM processItem;

            if (processItem = PhReferenceProcessItem(processId))
            {
                PhMwpExecuteProcessPriorityCommand(MenuItem->Id, &processItem, 1);
                PhDereferenceObject(processItem);
            }
            else
            {
                PhShowError(PhMainWndHandle, L"The process does not exist.");
            }
        }
        break;
    case ID_IOPRIORITY_HIGH:
    case ID_IOPRIORITY_NORMAL:
    case ID_IOPRIORITY_LOW:
    case ID_IOPRIORITY_VERYLOW:
        {
            HANDLE processId = MenuItem->Context;
            PPH_PROCESS_ITEM processItem;

            if (processItem = PhReferenceProcessItem(processId))
            {
                PhMwpExecuteProcessIoPriorityCommand(MenuItem->Id, &processItem, 1);
                PhDereferenceObject(processItem);
            }
            else
            {
                PhShowError(PhMainWndHandle, L"The process does not exist.");
            }
        }
        break;
    }

    return FALSE;
}

VOID PhMwpAddIconProcesses(
    _In_ PPH_EMENU_ITEM Menu,
    _In_ ULONG NumberOfProcesses
    )
{
    ULONG i;
    PPH_PROCESS_ITEM *processItems;
    ULONG numberOfProcessItems;
    PPH_LIST processList;
    PPH_PROCESS_ITEM processItem;

    PhEnumProcessItems(&processItems, &numberOfProcessItems);
    processList = PhCreateList(numberOfProcessItems);
    PhAddItemsList(processList, processItems, numberOfProcessItems);

    // Remove non-real processes.
    for (i = 0; i < processList->Count; i++)
    {
        processItem = processList->Items[i];

        if (!PH_IS_REAL_PROCESS_ID(processItem->ProcessId))
        {
            PhRemoveItemList(processList, i);
            i--;
        }
    }

    // Remove processes with zero CPU usage and those running as other users.
    for (i = 0; i < processList->Count && processList->Count > NumberOfProcesses; i++)
    {
        processItem = processList->Items[i];

        if (
            processItem->CpuUsage == 0 ||
            !processItem->UserName ||
            (PhCurrentUserName && !PhEqualString(processItem->UserName, PhCurrentUserName, TRUE))
            )
        {
            PhRemoveItemList(processList, i);
            i--;
        }
    }

    // Sort the processes by CPU usage and remove the extra processes at the end of the list.
    qsort(processList->Items, processList->Count, sizeof(PVOID), IconProcessesCpuUsageCompare);

    if (processList->Count > NumberOfProcesses)
    {
        PhRemoveItemsList(processList, NumberOfProcesses, processList->Count - NumberOfProcesses);
    }

    // Lastly, sort by name.
    qsort(processList->Items, processList->Count, sizeof(PVOID), IconProcessesNameCompare);

    // Delete all menu items.
    PhRemoveAllEMenuItems(Menu);

    // Add the processes.

    for (i = 0; i < processList->Count; i++)
    {
        PPH_EMENU_ITEM subMenu;
        HBITMAP iconBitmap;
        CLIENT_ID clientId;
        PPH_STRING clientIdName;
        PPH_STRING escapedName;

        processItem = processList->Items[i];

        // Process

        clientId.UniqueProcess = processItem->ProcessId;
        clientId.UniqueThread = NULL;

        clientIdName = PH_AUTO(PhGetClientIdName(&clientId));
        escapedName = PH_AUTO(PhEscapeStringForMenuPrefix(&clientIdName->sr));

        subMenu = PhCreateEMenuItem(
            0,
            0,
            escapedName->Buffer,
            NULL,
            processItem->ProcessId
            );

        if (processItem->SmallIcon)
        {
            iconBitmap = PhIconToBitmap(processItem->SmallIcon, PhSmallIconSize.X, PhSmallIconSize.Y);
        }
        else
        {
            HICON icon;

            PhGetStockApplicationIcon(&icon, NULL);
            iconBitmap = PhIconToBitmap(icon, PhSmallIconSize.X, PhSmallIconSize.Y);
        }

        subMenu->Bitmap = iconBitmap;
        subMenu->Flags |= PH_EMENU_BITMAP_OWNED; // automatically destroy the bitmap when necessary

        PhAddMiniProcessMenuItems(subMenu, processItem->ProcessId);
        PhInsertEMenuItem(Menu, subMenu, -1);
    }

    PhDereferenceObject(processList);
    PhDereferenceObjects(processItems, numberOfProcessItems);
    PhFree(processItems);
}

VOID PhShowIconContextMenu(
    _In_ POINT Location
    )
{
    PPH_EMENU menu;
    PPH_EMENU_ITEM item;
    PH_PLUGIN_MENU_INFORMATION menuInfo;
    ULONG numberOfProcesses;
    ULONG id;
    ULONG i;

    // This function seems to be called recursively under some circumstances.
    // To reproduce:
    // 1. Hold right mouse button on tray icon, then left click.
    // 2. Make the menu disappear by clicking on the menu then clicking somewhere else.
    // So, don't store any global state or bad things will happen.

    menu = PhCreateEMenu();
    PhLoadResourceEMenuItem(menu, PhInstanceHandle, MAKEINTRESOURCE(IDR_ICON), 0);

    // Check the Notifications menu items.
    for (i = PH_NOTIFY_MINIMUM; i != PH_NOTIFY_MAXIMUM; i <<= 1)
    {
        if (PhMwpNotifyIconNotifyMask & i)
        {
            switch (i)
            {
            case PH_NOTIFY_PROCESS_CREATE:
                id = ID_NOTIFICATIONS_NEWPROCESSES;
                break;
            case PH_NOTIFY_PROCESS_DELETE:
                id = ID_NOTIFICATIONS_TERMINATEDPROCESSES;
                break;
            case PH_NOTIFY_SERVICE_CREATE:
                id = ID_NOTIFICATIONS_NEWSERVICES;
                break;
            case PH_NOTIFY_SERVICE_DELETE:
                id = ID_NOTIFICATIONS_DELETEDSERVICES;
                break;
            case PH_NOTIFY_SERVICE_START:
                id = ID_NOTIFICATIONS_STARTEDSERVICES;
                break;
            case PH_NOTIFY_SERVICE_STOP:
                id = ID_NOTIFICATIONS_STOPPEDSERVICES;
                break;
            }

            PhSetFlagsEMenuItem(menu, id, PH_EMENU_CHECKED, PH_EMENU_CHECKED);
        }
    }

    // Add processes to the menu.

    numberOfProcesses = PhGetIntegerSetting(L"IconProcesses");
    item = PhFindEMenuItem(menu, 0, L"Processes", 0);

    if (item)
        PhMwpAddIconProcesses(item, numberOfProcesses);

    // Fix up the Computer menu.
    PhMwpSetupComputerMenu(menu);

    // Give plugins a chance to modify the menu.

    if (PhPluginsEnabled)
    {
        PhPluginInitializeMenuInfo(&menuInfo, menu, PhMainWndHandle, 0);
        PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackIconMenuInitializing), &menuInfo);
    }

    SetForegroundWindow(PhMainWndHandle); // window must be foregrounded so menu will disappear properly
    item = PhShowEMenu(
        menu,
        PhMainWndHandle,
        PH_EMENU_SHOW_LEFTRIGHT,
        PH_ALIGN_LEFT | PH_ALIGN_TOP,
        Location.x,
        Location.y
        );

    if (item)
    {
        BOOLEAN handled = FALSE;

        if (PhPluginsEnabled && !handled)
            handled = PhPluginTriggerEMenuItem(&menuInfo, item);

        if (!handled)
            handled = PhHandleMiniProcessMenuItem(item);

        if (!handled)
            handled = PhMwpExecuteComputerCommand(item->Id);

        if (!handled)
        {
            switch (item->Id)
            {
            case ID_ICON_SHOWHIDEPROCESSHACKER:
                SendMessage(PhMainWndHandle, WM_PH_TOGGLE_VISIBLE, 0, 0);
                break;
            case ID_ICON_SYSTEMINFORMATION:
                SendMessage(PhMainWndHandle, WM_COMMAND, ID_VIEW_SYSTEMINFORMATION, 0);
                break;
            case ID_NOTIFICATIONS_ENABLEALL:
                PhMwpNotifyIconNotifyMask |= PH_NOTIFY_VALID_MASK;
                break;
            case ID_NOTIFICATIONS_DISABLEALL:
                PhMwpNotifyIconNotifyMask &= ~PH_NOTIFY_VALID_MASK;
                break;
            case ID_NOTIFICATIONS_NEWPROCESSES:
            case ID_NOTIFICATIONS_TERMINATEDPROCESSES:
            case ID_NOTIFICATIONS_NEWSERVICES:
            case ID_NOTIFICATIONS_STARTEDSERVICES:
            case ID_NOTIFICATIONS_STOPPEDSERVICES:
            case ID_NOTIFICATIONS_DELETEDSERVICES:
                {
                    ULONG bit;

                    switch (item->Id)
                    {
                    case ID_NOTIFICATIONS_NEWPROCESSES:
                        bit = PH_NOTIFY_PROCESS_CREATE;
                        break;
                    case ID_NOTIFICATIONS_TERMINATEDPROCESSES:
                        bit = PH_NOTIFY_PROCESS_DELETE;
                        break;
                    case ID_NOTIFICATIONS_NEWSERVICES:
                        bit = PH_NOTIFY_SERVICE_CREATE;
                        break;
                    case ID_NOTIFICATIONS_STARTEDSERVICES:
                        bit = PH_NOTIFY_SERVICE_START;
                        break;
                    case ID_NOTIFICATIONS_STOPPEDSERVICES:
                        bit = PH_NOTIFY_SERVICE_STOP;
                        break;
                    case ID_NOTIFICATIONS_DELETEDSERVICES:
                        bit = PH_NOTIFY_SERVICE_DELETE;
                        break;
                    }

                    PhMwpNotifyIconNotifyMask ^= bit;
                }
                break;
            case ID_ICON_EXIT:
                SendMessage(PhMainWndHandle, WM_COMMAND, ID_HACKER_EXIT, 0);
                break;
            }
        }
    }

    PhDestroyEMenu(menu);
}

VOID PhShowIconNotification(
    _In_ PWSTR Title,
    _In_ PWSTR Text,
    _In_ ULONG Flags
    )
{
    PhNfShowBalloonTip(0, Title, Text, 10, Flags);
}

VOID PhShowDetailsForIconNotification(
    VOID
    )
{
    switch (PhMwpLastNotificationType)
    {
    case PH_NOTIFY_PROCESS_CREATE:
        {
            PPH_PROCESS_NODE processNode;

            if (processNode = PhFindProcessNode(PhMwpLastNotificationDetails.ProcessId))
            {
                ProcessHacker_SelectTabPage(PhMainWndHandle, PhMwpProcessesPage->Index);
                ProcessHacker_SelectProcessNode(PhMainWndHandle, processNode);
                ProcessHacker_ToggleVisible(PhMainWndHandle, TRUE);
            }
        }
        break;
    case PH_NOTIFY_SERVICE_CREATE:
    case PH_NOTIFY_SERVICE_START:
    case PH_NOTIFY_SERVICE_STOP:
        {
            PPH_SERVICE_ITEM serviceItem;

            if (PhMwpLastNotificationDetails.ServiceName &&
                (serviceItem = PhReferenceServiceItem(PhMwpLastNotificationDetails.ServiceName->Buffer)))
            {
                ProcessHacker_SelectTabPage(PhMainWndHandle, PhMwpServicesPage->Index);
                ProcessHacker_SelectServiceItem(PhMainWndHandle, serviceItem);
                ProcessHacker_ToggleVisible(PhMainWndHandle, TRUE);

                PhDereferenceObject(serviceItem);
            }
        }
        break;
    }
}

VOID PhMwpClearLastNotificationDetails(
    VOID
    )
{
    if (PhMwpLastNotificationType &
        (PH_NOTIFY_SERVICE_CREATE | PH_NOTIFY_SERVICE_DELETE | PH_NOTIFY_SERVICE_START | PH_NOTIFY_SERVICE_STOP))
    {
        PhClearReference(&PhMwpLastNotificationDetails.ServiceName);
    }

    PhMwpLastNotificationType = 0;
    memset(&PhMwpLastNotificationDetails, 0, sizeof(PhMwpLastNotificationDetails));
}

BOOLEAN PhMwpPluginNotifyEvent(
    _In_ ULONG Type,
    _In_ PVOID Parameter
    )
{
    PH_PLUGIN_NOTIFY_EVENT notifyEvent;

    notifyEvent.Type = Type;
    notifyEvent.Handled = FALSE;
    notifyEvent.Parameter = Parameter;

    PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackNotifyEvent), &notifyEvent);

    return notifyEvent.Handled;
}

VOID PhMwpUpdateUsersMenu(
    _In_ PPH_EMENU UsersMenu
    )
{
    PSESSIONIDW sessions;
    ULONG numberOfSessions;
    ULONG i;

    if (WinStationEnumerateW(NULL, &sessions, &numberOfSessions))
    {
        for (i = 0; i < numberOfSessions; i++)
        {
            PPH_EMENU_ITEM userMenu;
            PPH_STRING menuText;
            PPH_STRING escapedMenuText;
            WINSTATIONINFORMATION winStationInfo;
            ULONG returnLength;

            if (!WinStationQueryInformationW(
                NULL,
                sessions[i].SessionId,
                WinStationInformation,
                &winStationInfo,
                sizeof(WINSTATIONINFORMATION),
                &returnLength
                ))
            {
                winStationInfo.Domain[0] = 0;
                winStationInfo.UserName[0] = 0;
            }

            if (winStationInfo.Domain[0] == 0 || winStationInfo.UserName[0] == 0)
            {
                // Probably the Services or RDP-Tcp session.
                continue;
            }

            menuText = PhFormatString(
                L"%u: %s\\%s",
                sessions[i].SessionId,
                winStationInfo.Domain,
                winStationInfo.UserName
                );
            escapedMenuText = PhEscapeStringForMenuPrefix(&menuText->sr);
            PhDereferenceObject(menuText);

            PhInsertEMenuItem(UsersMenu, userMenu = PhCreateEMenuItem(0, IDR_USER, escapedMenuText->Buffer, NULL, UlongToPtr(sessions[i].SessionId)), -1);
            PhLoadResourceEMenuItem(userMenu, PhInstanceHandle, MAKEINTRESOURCE(IDR_USER), 0);

            PhAutoDereferenceObject(escapedMenuText);
        }

        WinStationFreeMemory(sessions);
    }
}
