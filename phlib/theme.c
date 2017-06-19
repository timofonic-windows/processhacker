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

#pragma comment(lib, "dwmapi.lib")

#include <ph.h>
#include <phtheme.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <VSStyle.h>
#include <emenu.h>
#include <settings.h>
#include <windowsx.h>

LRESULT CALLBACK PhpCustomFrameWndSubclassProc(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ UINT_PTR uIdSubclass,
    _In_ ULONG_PTR dwRefData
    )
{
    //LRESULT result;
    //if (DwmDefWindowProc(hWnd, uMsg, wParam, lParam, &result))
    //    return result;

    switch (uMsg)
    {
    case WM_DESTROY:
        RemoveWindowSubclass(hWnd, PhpCustomFrameWndSubclassProc, uIdSubclass);
        break;
    case WM_NCACTIVATE:
        // Force paint our non-client area otherwise Windows will paint its own.
        //RedrawWindow(hWnd, NULL, NULL, RDW_UPDATENOW);
        break;
    //case WM_NCCALCSIZE:
    //    {
    //        LPNCCALCSIZE_PARAMS ncsize = (NCCALCSIZE_PARAMS*)lParam;

    //        // Let Windows handle the non-client defaults.
    //        //DefSubclassProc(hWnd, uMsg, wParam, lParam);

    //        // Deflate the client area to accommodate the custom frame.
    //        //ncsize->rgrc[0].left = ncsize->rgrc[0].left + 5;
    //        ncsize->rgrc[0].top += 15;
    //        //ncsize->rgrc[0].right = ncsize->rgrc[0].right - 5;
    //        //ncsize->rgrc[0].bottom = ncsize->rgrc[0].bottom - 5;
    //    }
    //    return 0;
    case WM_NCPAINT:
        {
            RECT windowRect;
    
            // Let Windows handle the non-client defaults.
            DefSubclassProc(hWnd, uMsg, wParam, lParam);
            // Get the screen coordinates of the window.
            GetWindowRect(hWnd, &windowRect);
            // Adjust the coordinates (start from 0,0).
            OffsetRect(&windowRect, -windowRect.left, -windowRect.top);
            // Get the position of the inserted button.
            //PhpSearchGetButtonRect(context, &windowRect);
            // Draw the button.
            //PhpSearchDrawButton(context, windowRect);

            HRGN updateRegion;

            updateRegion = (HRGN)wParam;

            if (updateRegion == (HRGN)1) // HRGN_FULL
                updateRegion = NULL;

            // Themed border
            //if (context->Style & WS_BORDER)
//            {
                HDC hdc;
                ULONG flags;
//                RECT rect;

                // Note the use of undocumented flags below. GetDCEx doesn't work without these.

                flags = DCX_WINDOW | DCX_LOCKWINDOWUPDATE | 0x10000;

                if (updateRegion)
                    flags |= DCX_INTERSECTRGN | 0x40000;

                if (hdc = GetDCEx(hWnd, updateRegion, flags))
                {
                    SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                    SetDCBrushColor(hdc, RGB(0, 0, 0));
                    FillRect(hdc, &windowRect, GetStockObject(DC_BRUSH));

                    ReleaseDC(hWnd, hdc);
                }
            //}
        }
        return 0;
    case WM_NCHITTEST:
        {
            //LRESULT l_result = 0;
            //ULONG position;
            //POINT point;

            //position = GetMessagePos();
            //point.x = GET_X_LPARAM(lParam);
            //point.y = GET_Y_LPARAM(lParam);
            //ScreenToClient(hWnd, &point);
            //
            //int nShift = GetSystemMetrics(SM_CXSMICON);
            //int nFrame = 2;

            //RECT wr;

            //GetWindowRect(hWnd, &wr);

            //int nWidth = wr.right - wr.left;

            //if (point.x <= nFrame && point.y <= nShift)
            //    l_result = HTTOPLEFT;
            //else if (point.y <= nFrame && point.x <= nShift)
            //    l_result = HTTOPLEFT;
            //else if (point.x >= (nWidth - nFrame) && point.y <= nShift)
            //    l_result = HTTOPRIGHT;
            //else if (point.x >= (nWidth - nFrame - nShift) && point.y <= nFrame)
            //    l_result = HTTOPRIGHT;
            //else if (point.x > nFrame && point.x <= (nFrame + GetSystemMetrics(SM_CXSMICON)))
            //    l_result = HTSYSMENU;
            //else if (point.x <= nFrame)
            //    l_result = HTLEFT;
            //else if (point.x >= (nWidth - nFrame))
            //    l_result = HTRIGHT;
            //else if (point.y <= nFrame)
            //    l_result = HTTOP;
            //else
            //    l_result = HTCAPTION;

            //return l_result;
        }
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


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
                {
                    LPNMCUSTOMDRAW customDraw = (LPNMCUSTOMDRAW)lParam;
                    WCHAR className[256];

                    if (!GetClassName(customDraw->hdr.hwndFrom, className, ARRAYSIZE(className)))
                        className[0] = 0;

                    if (PhEqualStringZ(className, L"Button", FALSE))
                    {
                        return PhThemeDrawButton(customDraw);
                    }
                    if (PhEqualStringZ(className, L"ReBarWindow32", FALSE))
                    {
                        return PhThemeWindowDrawRebar(customDraw);
                    }
                    else if (PhEqualStringZ(className, L"ToolbarWindow32", FALSE))
                    {
                        return PhThemeWindowDrawToolbar((LPNMTBCUSTOMDRAW)customDraw);
                    }
                    else if (PhEqualStringZ(className, L"SysTabControl32", FALSE))
                    {
                       
                    }
                    else if (PhEqualStringZ(className, L"SysLink", FALSE))
                    {

                    }
         
                    //LIST_VIEW_CUSTOM_DRAW lpNMCustomDraw = (LPNMLVCUSTOMDRAW) lParam;
                    //TOOL_TIPS_CUSTOM_DRAW lpNMCustomDraw = (LPNMTTCUSTOMDRAW) lParam;
                    //TREE_VIEW_CUSTOM_DRAW lpNMCustomDraw = (LPNMTVCUSTOMDRAW) lParam;
                    //TOOL_BAR_CUSTOM_DRAW lpNMCustomDraw = (LPNMTBCUSTOMDRAW) lParam;
                    // else LPNMCUSTOMDRAW
                }
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
        if (PhThemeWindowMeasureItem((LPMEASUREITEMSTRUCT)lParam))
            return TRUE;
        break;
    case WM_DRAWITEM:
        if (PhThemeWindowDrawItem((LPDRAWITEMSTRUCT)lParam))
            return TRUE;
        break;
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
        if (!GetParent(WindowHandle))
            SetWindowSubclass(WindowHandle, PhpCustomFrameWndSubclassProc, 0, 0);

        PhThemeInitializeWindow(WindowHandle);
    }
    else if (PhEqualStringZ(className, L"SysTabControl32", FALSE))
    {
        PhThemeInitializeTabWindow(WindowHandle);
    }

    return TRUE;
}

static BOOL CALLBACK PhpUpdateThemeWindowEnumProc(
    _In_ HWND WindowHandle,
    _In_ LPARAM lParam
    )
{
    PhThemeEnumChildWindows(
        WindowHandle,
        0x1000,
        PhpUpdateThemeWindowEnumProc,
        0
        );

    InvalidateRect(WindowHandle, NULL, FALSE);

    return TRUE;
}

VOID PhThemeInitializeWindow(
    _In_ HWND WindowHandle
    )
{
    if (!GetParent(WindowHandle))
    {
        ULONG policy = DWMNCRP_ENABLED;
        DwmSetWindowAttribute(WindowHandle, DWMWA_NCRENDERING_POLICY, &policy, sizeof(ULONG));
        SetWindowPos(WindowHandle, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
        RedrawWindow(WindowHandle, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);

        SetWindowSubclass(WindowHandle, PhpCustomFrameWndSubclassProc, 0, 0);
    }

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

VOID PhThemeReInitializeWindow(
    _In_ HWND WindowHandle
    )
{
    PhThemeEnumChildWindows(
        WindowHandle,
        0x1000,
        PhpUpdateThemeWindowEnumProc,
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
    _In_ LPNMCUSTOMDRAW drawInfo
    )
{
    BOOLEAN isGrayed = (drawInfo->uItemState & CDIS_GRAYED) == CDIS_GRAYED;
    BOOLEAN isChecked = (drawInfo->uItemState & CDIS_CHECKED) == CDIS_CHECKED;
    BOOLEAN isDisabled = (drawInfo->uItemState & CDIS_DISABLED) == CDIS_DISABLED;
    BOOLEAN isSelected = (drawInfo->uItemState & CDIS_SELECTED) == CDIS_SELECTED;
    BOOLEAN isHighlighted = (drawInfo->uItemState & CDIS_HOT) == CDIS_HOT;
    BOOLEAN isFocused = (drawInfo->uItemState & CDIS_FOCUS) == CDIS_FOCUS;

    if (drawInfo->dwDrawStage == CDDS_PREPAINT)
    {
        SetBkMode(drawInfo->hdc, TRANSPARENT);

        switch (PhGetIntegerSetting(L"GraphColorMode"))
        {
        case 0: // New colors
            return CDRF_DODEFAULT;
        case 1: // Old colors
            break;
        }

        if (isSelected)
        {
            SetBkColor(drawInfo->hdc, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(drawInfo->hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetDCBrushColor(drawInfo->hdc, RGB(78, 78, 78)); // RGB(65, 65, 65)));
            FillRect(drawInfo->hdc, &drawInfo->rc, GetStockObject(DC_BRUSH));
        }
        else if (isHighlighted)
        {
            SetBkColor(drawInfo->hdc, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(drawInfo->hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetDCBrushColor(drawInfo->hdc, RGB(42, 42, 42)); // RGB(78, 78, 78));
            FillRect(drawInfo->hdc, &drawInfo->rc, GetStockObject(DC_BRUSH));
        }
        else
        {
            SetBkColor(drawInfo->hdc, GetSysColor(COLOR_WINDOW));
            SetTextColor(drawInfo->hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetDCBrushColor(drawInfo->hdc, RGB(65, 65, 65)); //RGB(28, 28, 28)); // RGB(65, 65, 65));
            FillRect(drawInfo->hdc, &drawInfo->rc, GetStockObject(DC_BRUSH));  
        }
    }

    return CDRF_DODEFAULT;
}

BOOLEAN PhThemeWindowDrawItem(
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

    if (DrawInfo->CtlType == ODT_STATIC)
    {

    }
    else if (DrawInfo->CtlType == ODT_MENU)
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
            //SetBkColor(drawInfo->hDC, GetSysColor(COLOR_GRAYTEXT));

            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_WINDOWTEXT));
                SetDCBrushColor(DrawInfo->hDC, GetSysColor(COLOR_GRAYTEXT));
                break;
            case 1: // Old colors
                SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
                SetDCBrushColor(DrawInfo->hDC, GetSysColor(COLOR_GRAYTEXT));
                break;
            }

            FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
        }
        else if (isSelected)
        {
            //SetBkColor(drawInfo->hDC, GetSysColor(COLOR_HIGHLIGHT));

            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_WINDOWTEXT));
                break;
            case 1: // Old colors
                SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
                break;
            }

            SetDCBrushColor(DrawInfo->hDC, RGB(78, 78, 78));
            FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
        }
        else
        {
            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_WINDOWTEXT));
                SetDCBrushColor(DrawInfo->hDC, RGB(0xff, 0xff, 0xff));
                FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
                break;
            case 1: // Old colors
                //SetBkColor(drawInfo->hDC, GetSysColor(COLOR_WINDOW));
                SetTextColor(DrawInfo->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
                SetDCBrushColor(DrawInfo->hDC, RGB(28, 28, 28));
                FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
                break;
            }
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
            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetDCBrushColor(DrawInfo->hDC, RGB(0xff, 0xff, 0xff));
                FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
                break;
            case 1: // Old colors
                SetDCBrushColor(DrawInfo->hDC, RGB(28, 28, 28));
                FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
                break;
            }

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

            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetDCBrushColor(DrawInfo->hDC, RGB(0xff, 0xff, 0xff));
                FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
                break;
            case 1: // Old colors
                SetDCBrushColor(DrawInfo->hDC, RGB(78, 78, 78));
                FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));
                break;
            }

            //DrawEdge(drawInfo->hDC, &drawInfo->rcItem, BDR_RAISEDINNER, BF_TOP);
        }
        else //if ((menuItemInfo->Flags & PH_EMENU_TEXT_OWNED) == PH_EMENU_TEXT_OWNED)
        {
            PH_STRINGREF part;
            PH_STRINGREF firstPart;
            PH_STRINGREF secondPart;

            PhInitializeStringRef(&part, menuItemInfo->Text);
            PhSplitStringRefAtLastChar(&part, '\b', &firstPart, &secondPart);

            //SetDCBrushColor(DrawInfo->hDC, RGB(28, 28, 28));
            //FillRect(DrawInfo->hDC, &DrawInfo->rcItem, GetStockObject(DC_BRUSH));

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

        return TRUE;
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

        return TRUE;
    }

    return FALSE;
}

BOOLEAN PhThemeWindowMeasureItem(
    _In_ PMEASUREITEMSTRUCT DrawInfo
    )
{
    if (DrawInfo->CtlType == ODT_MENU)
    {
        PPH_EMENU_ITEM menuItemInfo = (PPH_EMENU_ITEM)DrawInfo->itemData;
        DrawInfo->itemWidth = 150;
        DrawInfo->itemHeight = 26;

        if ((menuItemInfo->Flags & PH_EMENU_SEPARATOR) == PH_EMENU_SEPARATOR)
        {
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

        return TRUE;
    }

    return FALSE;
}

LRESULT PhThemeWindowDrawRebar(
    _In_ LPNMCUSTOMDRAW DrawInfo
    )
{
    switch (DrawInfo->dwDrawStage)
    {
    case CDDS_PREPAINT:
        {
            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetTextColor(DrawInfo->hdc, RGB(0x0, 0x0, 0x0));
                SetDCBrushColor(DrawInfo->hdc, RGB(0xff, 0xff, 0xff));
                break;
            case 1: // Old colors
                SetTextColor(DrawInfo->hdc, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor(DrawInfo->hdc, RGB(65, 65, 65));
                break;
            }

            FillRect(DrawInfo->hdc, &DrawInfo->rc, GetStockObject(DC_BRUSH));
        }
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        {
            switch (PhGetIntegerSetting(L"GraphColorMode"))
            {
            case 0: // New colors
                SetTextColor(DrawInfo->hdc, RGB(0x0, 0x0, 0x0));
                SetDCBrushColor(DrawInfo->hdc, RGB(0x0, 0x0, 0x0));
            case 1: // Old colors
                SetTextColor(DrawInfo->hdc, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor(DrawInfo->hdc, RGB(65, 65, 65));
                break;
            }

            FillRect(DrawInfo->hdc, &DrawInfo->rc, GetStockObject(DC_BRUSH));
        }
        return CDRF_SKIPDEFAULT;
    }

    return CDRF_DODEFAULT;
}

LRESULT PhThemeWindowDrawToolbar(
    _In_ LPNMTBCUSTOMDRAW DrawInfo
    )
{
    switch (DrawInfo->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    case CDDS_ITEMPREPAINT:
        {
            TBBUTTONINFO buttonInfo =
            {
                sizeof(TBBUTTONINFO),
                TBIF_STYLE | TBIF_COMMAND | TBIF_STATE | TBIF_IMAGE
            };

            SetBkMode(DrawInfo->nmcd.hdc, TRANSPARENT);

            ULONG currentIndex = (ULONG)SendMessage(
                DrawInfo->nmcd.hdr.hwndFrom,
                TB_COMMANDTOINDEX,
                DrawInfo->nmcd.dwItemSpec,
                0
                );
            BOOLEAN isHighlighted = SendMessage(
                DrawInfo->nmcd.hdr.hwndFrom,
                TB_GETHOTITEM,
                0,
                0
                ) == currentIndex;
            BOOLEAN isMouseDown = SendMessage(
                DrawInfo->nmcd.hdr.hwndFrom,
                TB_ISBUTTONPRESSED,
                DrawInfo->nmcd.dwItemSpec,
                0
                ) == 0;

            /*if (isMouseDown)
            {
                SetTextColor(DrawInfo->nmcd.hdc, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor(DrawInfo->nmcd.hdc, RGB(0xff, 0xff, 0xff));
                FillRect(DrawInfo->nmcd.hdc, &DrawInfo->nmcd.rc, GetStockObject(DC_BRUSH));
            }*/

            if (isHighlighted)
            {
                switch (PhGetIntegerSetting(L"GraphColorMode"))
                {
                case 0: // New colors
                    SetTextColor(DrawInfo->nmcd.hdc, RGB(0x0, 0x0, 0x0));
                    SetDCBrushColor(DrawInfo->nmcd.hdc, RGB(0xff, 0xff, 0xff));
                    FillRect(DrawInfo->nmcd.hdc, &DrawInfo->nmcd.rc, GetStockObject(DC_BRUSH));
                    break;
                case 1: // Old colors
                    SetTextColor(DrawInfo->nmcd.hdc, RGB(0xff, 0xff, 0xff));
                    SetDCBrushColor(DrawInfo->nmcd.hdc, RGB(128, 128, 128));
                    FillRect(DrawInfo->nmcd.hdc, &DrawInfo->nmcd.rc, GetStockObject(DC_BRUSH));
                    break;
                }
            }
            else
            {
                switch (PhGetIntegerSetting(L"GraphColorMode"))
                {
                case 0: // New colors
                    SetTextColor(DrawInfo->nmcd.hdc, RGB(0x0, 0x0, 0x0));
                    SetDCBrushColor(DrawInfo->nmcd.hdc, RGB(0xff, 0xff, 0xff));
                    FillRect(DrawInfo->nmcd.hdc, &DrawInfo->nmcd.rc, GetStockObject(DC_BRUSH));
                    break;
                case 1: // Old colors
                    SetTextColor(DrawInfo->nmcd.hdc, RGB(0xff, 0xff, 0xff));
                    SetDCBrushColor(DrawInfo->nmcd.hdc, RGB(65, 65, 65)); //RGB(28, 28, 28)); // RGB(65, 65, 65));
                    FillRect(DrawInfo->nmcd.hdc, &DrawInfo->nmcd.rc, GetStockObject(DC_BRUSH));
                    break;
                }
            }


            static HFONT fontHandle = NULL;
            if (!fontHandle)
            {
                NONCLIENTMETRICS metrics = { sizeof(metrics) };
                if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &metrics, 0))
                {
                    fontHandle = CreateFontIndirect(&metrics.lfMessageFont);
                }
            }
            SelectObject(DrawInfo->nmcd.hdc, fontHandle);


            if (SendMessage(
                DrawInfo->nmcd.hdr.hwndFrom,
                TB_GETBUTTONINFO,
                (ULONG)DrawInfo->nmcd.dwItemSpec,
                (LPARAM)&buttonInfo
                ) == -1)
            {
                break;
            }

            if (buttonInfo.iImage != -1)
            {
                HIMAGELIST toolbarImageList;

                if (toolbarImageList = (HIMAGELIST)SendMessage(
                    DrawInfo->nmcd.hdr.hwndFrom, 
                    TB_GETIMAGELIST, 
                    0, 
                    0
                    ))
                {
                    DrawInfo->nmcd.rc.left += 5;

                    ImageList_Draw(
                        toolbarImageList,
                        buttonInfo.iImage,
                        DrawInfo->nmcd.hdc,
                        DrawInfo->nmcd.rc.left,
                        DrawInfo->nmcd.rc.top + 3,
                        ILD_NORMAL
                        );
                }
            }

            if (buttonInfo.fsStyle & BTNS_SHOWTEXT)
            {
                WCHAR buttonText[0x100] = L"";

                SendMessage(
                    DrawInfo->nmcd.hdr.hwndFrom,
                    TB_GETBUTTONTEXT,
                    (ULONG)DrawInfo->nmcd.dwItemSpec,
                    (LPARAM)buttonText
                    );

                DrawInfo->nmcd.rc.left += 10;
                DrawText(
                    DrawInfo->nmcd.hdc,
                    buttonText,
                    -1,
                    &DrawInfo->nmcd.rc,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX
                    );
            }

            //DrawInfo->clrText = RGB(0x0, 0xff, 0);
            //return TBCDRF_USECDCOLORS | CDRF_NEWFONT;
        }
        return CDRF_SKIPDEFAULT;
    }

    return CDRF_DODEFAULT;
}