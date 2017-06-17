/*
* Process Hacker -
*   Theme support
*
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

#include <ph.h>
#include <phtheme.h>
#include <uxtheme.h>
#include <VSStyle.h>
#include <emenu.h>
#include <settings.h>

LRESULT CALLBACK PhpMainThemeWndSubclassProc(
    _In_ HWND hwnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ UINT_PTR uIdSubclass,
    _In_ ULONG_PTR dwRefData
    )
{
    switch (uMsg)
    {
    case WM_DESTROY:
        RemoveWindowSubclass(hwnd, PhpMainThemeWndSubclassProc, uIdSubclass);
        break;
    case WM_NOTIFY:
        {
            LPNMHDR data = (LPNMHDR)lParam;

            switch (data->code)
            {
            case NM_CUSTOMDRAW:
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PhThemeDrawButton((LPNMTVCUSTOMDRAW)lParam));
                return TRUE;
            }
        }
        break;
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        {
            SetBkMode((HDC)wParam, TRANSPARENT);

            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetTextColor((HDC)wParam, RGB(0x0, 0x0, 0x0));
                SetDCBrushColor((HDC)wParam, RGB(0xff, 0xff, 0xff));
                return (INT_PTR)GetStockObject(DC_BRUSH);
            case 1: // Old colors
                SetTextColor((HDC)wParam, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor((HDC)wParam, RGB(30, 30, 30));
                return (INT_PTR)GetStockObject(DC_BRUSH);
            }
        }
        break;
    case WM_MEASUREITEM:
        PhThemeWindowMeasureItem((LPMEASUREITEMSTRUCT)lParam);
        return TRUE;
    case WM_DRAWITEM:
        PhThemeWindowDrawItem((LPDRAWITEMSTRUCT)lParam);
        return TRUE;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK PhpTabControlWndSubclassProc(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ UINT_PTR uIdSubclass,
    _In_ ULONG_PTR dwRefData
    )
{
    static BOOLEAN MouseActive = FALSE;

    switch (uMsg)
    {
    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, PhpTabControlWndSubclassProc, uIdSubclass);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        {
            POINT pt;
            RECT rcItem;
            TCITEM tcItem;
            tcItem.mask = TCIF_STATE;
            tcItem.dwStateMask = TCIS_HIGHLIGHTED;

            GetCursorPos(&pt);
            MapWindowPoints(NULL, hWnd, &pt, 1);

            int nCount = TabCtrl_GetItemCount(hWnd);

            for (int i = 0; i < nCount; i++)
            {
                TabCtrl_GetItemRect(hWnd, i, &rcItem);
                tcItem.dwState = PtInRect(&rcItem, pt) ? TCIS_HIGHLIGHTED : 0;
                TabCtrl_SetItem(hWnd, i, &tcItem);
            }


            if (!MouseActive)
            {
                TRACKMOUSEEVENT trackEvent =
                { 
                    sizeof(TRACKMOUSEEVENT),
                    TME_LEAVE, 
                    hWnd,
                    0 
                };

                TrackMouseEvent(&trackEvent);
                MouseActive = TRUE;
            }         

            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        {
            TCITEM tcItem = 
            { 
                TCIF_STATE,
                0, 
                TCIS_HIGHLIGHTED 
            };

            int nCount = TabCtrl_GetItemCount(hWnd);

            for (int i = 0; i < nCount; i++)
            {
                TabCtrl_SetItem(hWnd, i, &tcItem);
            }

            InvalidateRect(hWnd, NULL, FALSE);
            MouseActive = FALSE;
        }
        break;
    case WM_PAINT:
        {
            //HTHEME hTheme = OpenThemeData(hWnd, L"TAB");
            WCHAR szText[MAX_PATH + 1];
            RECT rcClient, rcItem;
            TCITEMW tcItem;
            PAINTSTRUCT ps;

            BeginPaint(hWnd, &ps);

            GetClientRect(hWnd, &rcClient);

            TabCtrl_GetItemRect(hWnd, 0, &rcItem);

            ZeroMemory(&tcItem, sizeof(tcItem));
            tcItem.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_STATE;
            tcItem.dwStateMask = TCIS_BUTTONPRESSED | TCIS_HIGHLIGHTED;
            tcItem.pszText = szText;
            tcItem.cchTextMax = MAX_PATH;

            RECT rcSpin = rcClient;
            HWND hWndSpin = GetDlgItem(hWnd, 1);
            if (hWndSpin)
            {
                GetWindowRect(hWndSpin, &rcSpin);
                MapWindowPoints(NULL, hWnd, (LPPOINT)&rcSpin, 2);
                rcSpin.right = rcSpin.left;
                rcSpin.left = rcSpin.top = 0;
                rcSpin.bottom = rcClient.bottom;
            }

            HDC hdc = CreateCompatibleDC(ps.hdc);
            HBITMAP hbm = CreateCompatibleBitmap(ps.hdc, rcClient.right, rcClient.bottom);
            SelectObject(hdc, hbm);

            //DrawThemeParentBackground(hWnd, hdc, &rcClient);
            //DrawThemeBackground(hTheme, hdc, TABP_PANE, 0, &rcPane, NULL);
     
            SetBkMode(hdc, TRANSPARENT);

            static HFONT fontHandle = NULL;
            if (!fontHandle)
            {
                NONCLIENTMETRICS metrics = { sizeof(metrics) };
                if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &metrics, 0))
                {
                    fontHandle = CreateFontIndirect(&metrics.lfMessageFont);
                }
            }

            SelectObject(hdc, fontHandle);

            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                //SetTextColor(hdc, RGB(0x0, 0xff, 0x0));
                SetDCBrushColor(hdc, RGB(0xff, 0xff, 0xff));
                FillRect(hdc, &rcClient, GetStockObject(DC_BRUSH));
                break;
            case 1: // Old colors
                //SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor(hdc, RGB(65, 65, 65)); //RGB(28, 28, 28)); // RGB(65, 65, 65));
                FillRect(hdc, &rcClient, GetStockObject(DC_BRUSH));
                break;
            }
    

            HDC hdcTab = CreateCompatibleDC(hdc);

            int nCount = TabCtrl_GetItemCount(hWnd);
            for (int i = 0; i < nCount; i++)
            {
                TabCtrl_GetItemRect(hWnd, i, &rcItem);
                TabCtrl_GetItem(hWnd, i, &tcItem);

                POINT pt;
                GetCursorPos(&pt);
                MapWindowPoints(NULL, hWnd, &pt, 1);

                if (PtInRect(&rcItem, pt))
                {
                    OffsetRect(&rcItem, 2, 2);

                    switch (PhGetIntegerSetting(L"GraphColorMode"))
                    {
                    case 0: // New colors
                        {
                            if (TabCtrl_GetCurSel(hWnd) == i)
                            {
                                SetDCBrushColor(hdc, RGB(128, 128, 128));
                                FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                            }
                            else
                            {
                                //SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                                SetDCBrushColor(hdc, RGB(0xff, 0xff, 0xff));
                                FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                            }
                        }
                        break;
                    case 1: // Old colors
                        SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                        SetDCBrushColor(hdc, RGB(128, 128, 128));
                        FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                        break;
                    }

                    //FrameRect(hdc, &rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
                }
                else
                {
                    OffsetRect(&rcItem, 2, 2);

                    switch (PhGetIntegerSetting(L"GraphColorMode"))
                    {
                    case 0: // New colors
                        {
                            if (TabCtrl_GetCurSel(hWnd) == i)
                            {
                                SetDCBrushColor(hdc, RGB(128, 128, 128));
                                FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                            }
                            else
                            {
                                //SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                                SetDCBrushColor(hdc, RGB(0xff, 0xff, 0xff));
                                FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                            }
                        }
                        break;
                    case 1: // Old colors
                        {
                            if (TabCtrl_GetCurSel(hWnd) == i)
                            {
                                SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                                SetDCBrushColor(hdc, RGB(128, 128, 128));
                                FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                            }
                            else
                            {
                                SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                                SetDCBrushColor(hdc, RGB(64, 64, 64));
                                FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                            }
                        }
                        break;
                    }
                }

                DrawText(
                    hdc,
                    tcItem.pszText,
                    -1,
                    &rcItem,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX //| DT_WORD_ELLIPSIS
                    );
            }

            DeleteDC(hdcTab);

            BitBlt(ps.hdc, 0, 0, rcClient.right, rcClient.bottom, hdc, 0, 0, SRCCOPY);

            DeleteDC(hdc);
            DeleteObject(hbm);
            EndPaint(hWnd, &ps);

            InvalidateRect(hWnd, NULL, FALSE);
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK PhpHeaderWndSubclassProc(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ UINT_PTR uIdSubclass,
    _In_ ULONG_PTR dwRefData
    )
{
    static BOOLEAN MouseActive = FALSE;

    switch (uMsg)
    {
    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, PhpHeaderWndSubclassProc, uIdSubclass);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        {
            //POINT pt;
            //RECT rcItem;
            //TCITEM tcItem;
            //tcItem.mask = TCIF_STATE;
            //tcItem.dwStateMask = TCIS_HIGHLIGHTED;
            //GetCursorPos(&pt);
            //MapWindowPoints(NULL, hWnd, &pt, 1);
            //int nCount = TabCtrl_GetItemCount(hWnd);
            //for (int i = 0; i < nCount; i++)
            //{
            //    TabCtrl_GetItemRect(hWnd, i, &rcItem);
            //    tcItem.dwState = PtInRect(&rcItem, pt) ? TCIS_HIGHLIGHTED : 0;
            //    TabCtrl_SetItem(hWnd, i, &tcItem);
            //}

            if (!MouseActive)
            {
                TRACKMOUSEEVENT trackEvent =
                { 
                    sizeof(TRACKMOUSEEVENT), 
                    TME_LEAVE, 
                    hWnd, 
                    0 
                };

                TrackMouseEvent(&trackEvent);
                MouseActive = TRUE;
            }

            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        {
           // TCITEM tcItem = { TCIF_STATE, 0, TCIS_HIGHLIGHTED };
           //// int nCount = TabCtrl_GetItemCount(hWnd);
           // for (int i = 0; i < nCount; i++)
           // {
           //     //TabCtrl_SetItem(hWnd, i, &tcItem);
           // }
            InvalidateRect(hWnd, NULL, FALSE);
            MouseActive = FALSE;
        }
        break;
    case WM_PAINT:
        {
            //HTHEME hTheme = OpenThemeData(hWnd, L"HEADER");

            WCHAR szText[MAX_PATH + 1];
            RECT rcClient;
            HDITEM tcItem;
            PAINTSTRUCT ps;

            InvalidateRect(hWnd, NULL, FALSE);

            BeginPaint(hWnd, &ps);

            GetClientRect(hWnd, &rcClient);

            ZeroMemory(&tcItem, sizeof(tcItem));
            tcItem.mask = HDI_TEXT | HDI_IMAGE | HDI_STATE;
            tcItem.pszText = szText;
            tcItem.cchTextMax = MAX_PATH;

            HDC hdc = CreateCompatibleDC(ps.hdc);
            HBITMAP hbm = CreateCompatibleBitmap(ps.hdc, rcClient.right, rcClient.bottom);

            SelectObject(hdc, hbm);
    
            static HFONT fontHandle = NULL;
            if (!fontHandle)
            {
                NONCLIENTMETRICS metrics = { sizeof(metrics) };
                if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &metrics, 0))
                {
                    fontHandle = CreateFontIndirect(&metrics.lfMessageFont);
                }
            }
            SelectObject(hdc, fontHandle);

            SetBkMode(hdc, TRANSPARENT);

            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetTextColor(hdc, RGB(0x0, 0x0, 0x0));
                SetDCBrushColor(hdc, RGB(0xff, 0xff, 0xff));
                FillRect(hdc, &rcClient, GetStockObject(DC_BRUSH));
                break;
            case 1: // Old colors
                SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor(hdc, RGB(65, 65, 65)); //RGB(28, 28, 28)); // RGB(65, 65, 65));
                FillRect(hdc, &rcClient, GetStockObject(DC_BRUSH));
                break;
            }

            HDC hdcTab = CreateCompatibleDC(hdc);

            INT nCount = Header_GetItemCount(hWnd);
            for (INT i = 0; i < nCount; i++)
            {
                RECT headerRect;
                POINT pt;

                Header_GetItemRect(hWnd, i, &headerRect);
                Header_GetItem(hWnd, i, &tcItem);

                GetCursorPos(&pt);
                MapWindowPoints(NULL, hWnd, &pt, 1);

                if (PtInRect(&headerRect, pt))
                {
                    switch (PhGetIntegerSetting(L"GraphColorMode"))
                    {
                    case 0: // New colors
                        SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                        SetDCBrushColor(hdc, RGB(64, 64, 64));
                        FillRect(hdc, &headerRect, GetStockObject(DC_BRUSH));
                        break;
                    case 1: // Old colors
                        SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                        SetDCBrushColor(hdc, RGB(128, 128, 128));
                        FillRect(hdc, &headerRect, GetStockObject(DC_BRUSH));
                        break;
                    }
      
                    //FrameRect(hdc, &headerRect, GetSysColorBrush(COLOR_HIGHLIGHT));
                }
                else
                {
                    switch (PhGetIntegerSetting(L"GraphColorMode"))
                    {
                    case 0: // New colors
                        SetTextColor(hdc, RGB(0x0, 0x0, 0x0));
                        SetDCBrushColor(hdc, RGB(0xff, 0xff, 0xff));
                        FillRect(hdc, &headerRect, GetStockObject(DC_BRUSH));
                        break;
                    case 1: // Old colors
                        SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                        SetDCBrushColor(hdc, RGB(64, 64, 64));
                        FillRect(hdc, &headerRect, GetStockObject(DC_BRUSH));
                        break;
                    }

                    //FrameRect(hdc, &headerRect, GetSysColorBrush(COLOR_HIGHLIGHT));
                }

                DrawText(
                    hdc,
                    tcItem.pszText,
                    -1,
                    &headerRect,
                    DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX //| DT_WORD_ELLIPSIS
                    );
            }
  
            DeleteDC(hdcTab);

            BitBlt(ps.hdc, 0, 0, rcClient.right, rcClient.bottom, hdc, 0, 0, SRCCOPY);

            DeleteDC(hdc);
            DeleteObject(hbm);
            EndPaint(hWnd, &ps);
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK PhpStatusbarWndSubclassProc(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ UINT_PTR uIdSubclass,
    _In_ ULONG_PTR dwRefData
    )
{
    static BOOLEAN MouseActive = FALSE;

    switch (uMsg)
    {
    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, PhpStatusbarWndSubclassProc, uIdSubclass);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        {
            if (!MouseActive)
            {
                TRACKMOUSEEVENT trackEvent = 
                { 
                    sizeof(TRACKMOUSEEVENT), 
                    TME_LEAVE, 
                    hWnd, 
                    0 
                };

                TrackMouseEvent(&trackEvent);
                MouseActive = TRUE;
            }

            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        {
            InvalidateRect(hWnd, NULL, FALSE);
            MouseActive = FALSE;
        }
        break;
    case WM_PAINT:
        {
            RECT rcClient;
            PAINTSTRUCT ps;
            INT blockCoord[128];
            INT blockCount = (INT)SendMessage(hWnd, (UINT)SB_GETPARTS, (WPARAM)ARRAYSIZE(blockCoord), (WPARAM)blockCoord);

            InvalidateRect(hWnd, NULL, FALSE);

            BeginPaint(hWnd, &ps);

            GetClientRect(hWnd, &rcClient);

            HDC hdc = CreateCompatibleDC(ps.hdc);
            HBITMAP hbm = CreateCompatibleBitmap(ps.hdc, rcClient.right, rcClient.bottom);
           
            SetBkMode(hdc, TRANSPARENT);

            SelectObject(hdc, hbm);

            static HFONT fontHandle = NULL;
            if (!fontHandle)
            {
                NONCLIENTMETRICS metrics = { sizeof(metrics) };
                if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &metrics, 0))
                {
                    fontHandle = CreateFontIndirect(&metrics.lfMessageFont);
                }
            }
            SelectObject(hdc, fontHandle);


            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetTextColor(hdc, RGB(0x0, 0x0, 0x0));
                SetDCBrushColor(hdc, RGB(0xff, 0xff, 0xff));
                FillRect(hdc, &rcClient, GetStockObject(DC_BRUSH));
                break;
            case 1: // Old colors
                SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor(hdc, RGB(65, 65, 65)); //RGB(28, 28, 28)); // RGB(65, 65, 65));
                FillRect(hdc, &rcClient, GetStockObject(DC_BRUSH));
                break;
            }

            HDC hdcTab = CreateCompatibleDC(hdc);

            for (INT i = 0; i < blockCount; i++)
            {
                RECT blockRect;
                WCHAR buffer[260] = L"";
                SendMessage(hWnd, SB_GETRECT, (WPARAM)i, (WPARAM)&blockRect);
                SendMessage(hWnd, SB_GETTEXT, (WPARAM)i, (LPARAM)buffer);

                POINT pt;
                GetCursorPos(&pt);
                MapWindowPoints(NULL, hWnd, &pt, 1);

                if (PtInRect(&blockRect, pt))
                {
                    switch (PhGetIntegerSetting(L"GraphColorMode"))
                    {
                    case 0: // New colors
                        SetTextColor(hdc, RGB(0x0, 0x0, 0x0));
                        SetDCBrushColor(hdc, RGB(0xff, 0xff, 0xff));
                        FillRect(hdc, &blockRect, GetStockObject(DC_BRUSH));
                        //FrameRect(hdc, &blockRect, GetSysColorBrush(COLOR_HIGHLIGHT));
                        break;
                    case 1: // Old colors
                        SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                        SetDCBrushColor(hdc, RGB(128, 128, 128));
                        FillRect(hdc, &blockRect, GetStockObject(DC_BRUSH));
                        //FrameRect(hdc, &blockRect, GetSysColorBrush(COLOR_HIGHLIGHT));
                        break;
                    }
                }
                else
                {
                    switch (PhGetIntegerSetting(L"GraphColorMode"))
                    {
                    case 0: // New colors
                        SetTextColor(hdc, RGB(0x0, 0x0, 0x0));
                        SetDCBrushColor(hdc, RGB(0xff, 0xff, 0xff));
                        FillRect(hdc, &blockRect, GetStockObject(DC_BRUSH));
                        //FrameRect(hdc, &blockRect, GetSysColorBrush(COLOR_HIGHLIGHT));
                        break;
                    case 1: // Old colors
                        SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                        SetDCBrushColor(hdc, RGB(64, 64, 64));
                        FillRect(hdc, &blockRect, GetStockObject(DC_BRUSH));
                        //FrameRect(hdc, &blockRect, GetSysColorBrush(COLOR_HIGHLIGHT));
                        break;
                    }
                }

                DrawText(
                    hdc,
                    buffer,
                    -1,
                    &blockRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX //| DT_WORD_ELLIPSIS
                    );
            }

            DeleteDC(hdcTab);

            BitBlt(ps.hdc, 0, 0, rcClient.right, rcClient.bottom, hdc, 0, 0, SRCCOPY);

            DeleteDC(hdc);
            DeleteObject(hbm);
            EndPaint(hWnd, &ps);
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static BOOL CALLBACK PhpThemeWindowEnumWindowsProc(
    _In_ HWND WindowHandle,
    _In_ LPARAM lParam
    )
{
    WCHAR className[256];

    if (!GetClassName(WindowHandle, className, ARRAYSIZE(className)))
        className[0] = 0;

    if (PhEqualStringZ(className, L"Button", FALSE))
    {
        //ULONG windowStyle = (ULONG)GetWindowLongPtr(PhSipWindow, GWL_STYLE);
    }
    else if (PhEqualStringZ(className, L"#32770", FALSE))
    {
        PhThemeInitializeWindow(WindowHandle);
    }
    else if (PhEqualStringZ(className, L"SysTabControl32", FALSE))
    {
        PhThemeInitializeTabWindow(WindowHandle);
    }
    
    return TRUE;
}

static VOID PhThemeEnumChildWindows(
    _In_opt_ HWND hWnd,
    _In_ ULONG Limit,
    _In_ WNDENUMPROC Callback,
    _In_ LPARAM lParam
    )
{
    HWND childWindow = NULL;
    ULONG i = 0;

    while (i < Limit && (childWindow = FindWindowEx(hWnd, childWindow, NULL, NULL)))
    {
        if (!Callback(childWindow, lParam))
            return;

        i++;
    }
}

VOID PhThemeInitializeWindow(
    _In_ HWND WindowHandle
    )
{
    SetWindowSubclass(
        WindowHandle,
        PhpMainThemeWndSubclassProc,
        0,
        0
        );

    PhThemeEnumChildWindows(
        WindowHandle,
        0x1000,
        PhpThemeWindowEnumWindowsProc,
        0
        );

    InvalidateRect(WindowHandle, NULL, FALSE);
}

VOID PhThemeInitializeTabWindow(
    _In_ HWND TabWindowHandle
    )
{
    ULONG windowStyle = (ULONG)GetWindowLongPtr(TabWindowHandle, GWL_STYLE);

    SetWindowLongPtr(
        TabWindowHandle, 
        GWL_STYLE, 
        windowStyle | TCS_OWNERDRAWFIXED
        );

    SetWindowSubclass(
        TabWindowHandle, 
        PhpTabControlWndSubclassProc, 
        0, 
        0
        );

    InvalidateRect(TabWindowHandle, NULL, FALSE);
}

VOID PhThemeInitializeHeaderControl(
    _In_ HWND HeaderWindow
    )
{
    SetWindowSubclass(
        HeaderWindow, 
        PhpHeaderWndSubclassProc,
        0, 
        0
        );

    InvalidateRect(HeaderWindow, NULL, FALSE);
}

VOID PhThemeInitializeStatusbarControl(
    _In_ HWND StatusbarWindow
    )
{
    SetWindowSubclass(
        StatusbarWindow,
        PhpStatusbarWndSubclassProc,
        0,
        0
        );

    InvalidateRect(StatusbarWindow, NULL, FALSE);
}

LRESULT PhThemeDrawButton(
    _In_ LPNMTVCUSTOMDRAW drawInfo
    )
{
    BOOLEAN isGrayed = (drawInfo->nmcd.uItemState & CDIS_GRAYED) == CDIS_GRAYED;
    BOOLEAN isChecked = (drawInfo->nmcd.uItemState & CDIS_CHECKED) == CDIS_CHECKED;
    BOOLEAN isDisabled = (drawInfo->nmcd.uItemState & CDIS_DISABLED) == CDIS_DISABLED;
    BOOLEAN isSelected = (drawInfo->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED;
    BOOLEAN isHighlighted = (drawInfo->nmcd.uItemState & CDIS_HOT) == CDIS_HOT;
    BOOLEAN isFocused = (drawInfo->nmcd.uItemState & CDIS_FOCUS) == CDIS_FOCUS;

    if (drawInfo->nmcd.dwDrawStage == CDDS_PREPAINT)
    {
        SetBkMode(drawInfo->nmcd.hdc, TRANSPARENT);

        switch (PhGetIntegerSetting(L"GraphColorMode"))
        {
        case 0: // New colors
            return CDRF_DODEFAULT;
        case 1: // Old colors
            break;
        }

        if (isSelected)
        {
            SetBkColor(drawInfo->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(drawInfo->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetDCBrushColor(drawInfo->nmcd.hdc, RGB(78, 78, 78)); // RGB(65, 65, 65)));
            FillRect(drawInfo->nmcd.hdc, &drawInfo->nmcd.rc, GetStockObject(DC_BRUSH));
        }
        else if (isHighlighted)
        {
            SetBkColor(drawInfo->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(drawInfo->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetDCBrushColor(drawInfo->nmcd.hdc, RGB(42, 42, 42)); // RGB(78, 78, 78));
            FillRect(drawInfo->nmcd.hdc, &drawInfo->nmcd.rc, GetStockObject(DC_BRUSH));
        }
        else
        {
            SetBkColor(drawInfo->nmcd.hdc, GetSysColor(COLOR_WINDOW));
            SetTextColor(drawInfo->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetDCBrushColor(drawInfo->nmcd.hdc, RGB(65, 65, 65)); //RGB(28, 28, 28)); // RGB(65, 65, 65));
            FillRect(drawInfo->nmcd.hdc, &drawInfo->nmcd.rc, GetStockObject(DC_BRUSH));  
        }
    }

    return CDRF_DODEFAULT;
}

VOID PhThemeWindowDrawItem(
    _In_ PDRAWITEMSTRUCT DrawInfo
    )
{
    //LPDRAWITEMSTRUCT drawInfo = (LPDRAWITEMSTRUCT)lParam;
    BOOLEAN isGrayed = (DrawInfo->itemState & CDIS_GRAYED) == CDIS_GRAYED;
    BOOLEAN isChecked = (DrawInfo->itemState & CDIS_CHECKED) == CDIS_CHECKED;
    BOOLEAN isDisabled = (DrawInfo->itemState & CDIS_DISABLED) == CDIS_DISABLED;
    BOOLEAN isSelected = (DrawInfo->itemState & CDIS_SELECTED) == CDIS_SELECTED;
    //BOOLEAN isHighlighted = (drawInfo->itemState & CDIS_HOT) == CDIS_HOT;
    BOOLEAN isFocused = (DrawInfo->itemState & CDIS_FOCUS) == CDIS_FOCUS;
    //BOOLEAN isGrayed = (DrawInfo->itemState & ODS_GRAYED) == ODS_GRAYED;
    //BOOLEAN isChecked = (DrawInfo->itemState & ODS_CHECKED) == ODS_CHECKED;
    //BOOLEAN isDisabled = (DrawInfo->itemState & ODS_DISABLED) == ODS_DISABLED;
    //BOOLEAN isSelected = (DrawInfo->itemState & ODS_SELECTED) == ODS_SELECTED;
    BOOLEAN isHighlighted = (DrawInfo->itemState & ODS_HOTLIGHT) == ODS_HOTLIGHT;

    SetBkMode(DrawInfo->hDC, TRANSPARENT);

    switch (PhGetIntegerSetting(L"GraphColorMode"))
    {
    case 0: // New colors
        return;
    case 1: // Old colors
        break;
    }

    if (DrawInfo->CtlType == ODT_MENU)
    {
        PPH_EMENU_ITEM menuItemInfo = (PPH_EMENU_ITEM)DrawInfo->itemData;

        //FillRect(
        //    DrawInfo->hDC, 
        //    &DrawInfo->rcItem, 
        //    CreateSolidBrush(RGB(0, 0, 0))
        //    );
        //SetTextColor(drawInfo->hDC, RGB(0xff, 0xff, 0xff));

        if (isDisabled)
        {

        }
        else if (isSelected)
        {
            //SetBkColor(drawInfo->hDC, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetDCBrushColor(DrawInfo->hDC, RGB(78, 78, 78));
            FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
        }
        else
        {
            //SetBkColor(drawInfo->hDC, GetSysColor(COLOR_WINDOW));
            SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetDCBrushColor(DrawInfo->hDC, RGB(28, 28, 28));
            FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
        }

        if (isChecked)
        {
            //HFONT marlettFont = CreateFont(
            //    0, 0, 0, 0,
            //    FW_DONTCARE,
            //    FALSE,
            //    FALSE,
            //    FALSE,
            //    DEFAULT_CHARSET,
            //    OUT_OUTLINE_PRECIS,
            //    CLIP_DEFAULT_PRECIS,
            //    CLEARTYPE_QUALITY,
            //    VARIABLE_PITCH,
            //    L"Arial Unicode MS"
            //    );
            //HGDIOBJ originalFont = SelectObject(drawInfo->hDC, marlettFont);
            SetTextColor(DrawInfo->hDC, RGB(0xff, 0xff, 0xff));

            DrawInfo->rcItem.left += 8;
            DrawInfo->rcItem.top += 3;

            DrawText(
                DrawInfo->hDC,
                L"\u2713",
                -1,
                &DrawInfo->rcItem,
                DT_VCENTER | DT_NOCLIP
            );

            DrawInfo->rcItem.left -= 8;
            DrawInfo->rcItem.top -= 3;

            //SelectObject(drawInfo->hDC, originalFont);
            //DeleteObject(marlettFont);
        }

        if ((menuItemInfo->Flags & PH_EMENU_SEPARATOR) == PH_EMENU_SEPARATOR)
        {
            SetDCBrushColor(DrawInfo->hDC, RGB(78, 78, 78));
            //FillRect(DrawInfo->hDC, &drawInfo->rcItem, GetStockObject(DC_BRUSH));
            //DrawInfo->rcItem.top += 1;
            //DrawInfo->rcItem.bottom -= 2;
            //DrawFocusRect(drawInfo->hDC, &drawInfo->rcItem);

            SetRect(
                &DrawInfo->rcItem,
                DrawInfo->rcItem.left + 25,
                DrawInfo->rcItem.top + 2,
                DrawInfo->rcItem.right,
                DrawInfo->rcItem.bottom - 2
                );

            FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
            //DrawEdge(drawInfo->hDC, &drawInfo->rcItem, BDR_RAISEDINNER, BF_TOP);
        }
        else //if ((menuItemInfo->Flags & PH_EMENU_TEXT_OWNED) == PH_EMENU_TEXT_OWNED)
        {
            PH_STRINGREF part;
            PH_STRINGREF firstPart;
            PH_STRINGREF secondPart;

            PhInitializeStringRef(&part, menuItemInfo->Text);
            PhSplitStringRefAtLastChar(&part, '\b', &firstPart, &secondPart);

            if (menuItemInfo->Bitmap)
            {
                HDC hdcMem;
                BLENDFUNCTION blendFunction;

                blendFunction.BlendOp = AC_SRC_OVER;
                blendFunction.BlendFlags = 0;
                blendFunction.SourceConstantAlpha = 255;
                blendFunction.AlphaFormat = AC_SRC_ALPHA;

                hdcMem = CreateCompatibleDC(DrawInfo->hDC);
                SelectObject(hdcMem, menuItemInfo->Bitmap);

                GdiAlphaBlend(
                    DrawInfo->hDC,
                    DrawInfo->rcItem.left + 4,
                    DrawInfo->rcItem.top + 4,
                    16,
                    16,
                    hdcMem,
                    0,
                    0,
                    16,
                    16,
                    blendFunction
                );

                DeleteDC(hdcMem);
            }

            DrawInfo->rcItem.left += 25;
            DrawInfo->rcItem.right -= 25;

            DrawText(
                DrawInfo->hDC,
                firstPart.Buffer,
                (INT)firstPart.Length / 2,
                &DrawInfo->rcItem,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX | DT_NOCLIP //| DT_WORD_ELLIPSIS
                );

            DrawText(
                DrawInfo->hDC,
                secondPart.Buffer,
                (INT)secondPart.Length / 2,
                &DrawInfo->rcItem,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX | DT_NOCLIP //| DT_WORD_ELLIPSIS
                );
        }
    }
    else if (DrawInfo->CtlType == ODT_HEADER)
    {
        SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
        SetDCBrushColor(DrawInfo->hDC, RGB(30, 30, 30));
        FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
        DrawEdge(DrawInfo->hDC, &DrawInfo->rcItem, BDR_RAISEDOUTER, BF_RIGHT);

        HDITEM item;
        WCHAR buffer[260] = L"";

        item.mask = HDI_TEXT;
        item.cchTextMax = ARRAYSIZE(buffer);
        item.pszText = buffer;

        Header_GetItem(
            DrawInfo->hwndItem,
            DrawInfo->itemID,
            &item
            );

        DrawInfo->rcItem.left += 3;
        DrawText(
            DrawInfo->hDC,
            buffer,
            -1,
            &DrawInfo->rcItem,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX | DT_NOCLIP //| DT_WORD_ELLIPSIS
            );
        DrawInfo->rcItem.left -= 3;
    }
}

VOID PhThemeWindowMeasureItem(
    _In_ PMEASUREITEMSTRUCT DrawInfo
    )
{
    switch (PhGetIntegerSetting(L"GraphColorMode"))
    {
    case 0: // New colors
        return;
    case 1: // Old colors
        break;
    }

    if (DrawInfo->CtlType == ODT_MENU)
    {
        PPH_EMENU_ITEM menuItemInfo = (PPH_EMENU_ITEM)DrawInfo->itemData;
        DrawInfo->itemWidth = 150;
        DrawInfo->itemHeight = 26;

        if ((menuItemInfo->Flags & PH_EMENU_SEPARATOR) == PH_EMENU_SEPARATOR)
        {
            //drawInfo->itemHeight = 5;
            DrawInfo->itemHeight = GetSystemMetrics(SM_CYMENU) >> 2;
        }
        else
        {
            if (menuItemInfo->Text)
            {
                SIZE size;
                HDC hdc;

                hdc = CreateIC(L"DISPLAY", NULL, NULL, NULL);

                static HFONT fontHandle = NULL;
                if (!fontHandle)
                {
                    NONCLIENTMETRICS metrics = { sizeof(metrics) };
                    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &metrics, 0))
                    {
                        fontHandle = CreateFontIndirect(&metrics.lfMessageFont);
                    }
                }

                SelectObject(hdc, fontHandle);
 
                GetTextExtentPoint32(hdc, menuItemInfo->Text, (INT)wcslen(menuItemInfo->Text), &size);
                
                DrawInfo->itemWidth = size.cx + 90;
                //DrawInfo->itemHeight = size.cy;

                DeleteDC(hdc);
            }
        }
    }
}

