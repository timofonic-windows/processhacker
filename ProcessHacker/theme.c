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

#include <phapp.h>
#include "theme.h"
#include "phsettings.h"
#include "mainwnd.h"
#include <uxtheme.h>
#include <VSStyle.h>
#include "emenu.h"

LRESULT PhSipDialogThemeDrawButton(
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

LRESULT CALLBACK PhpThemeSubclassWndProc(
    _In_ HWND hwnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ UINT_PTR uIdSubclass,
    _In_ ULONG_PTR dwRefData
    )
{
    //PPH_SYSINFO_SECTION section = (PPH_SYSINFO_SECTION)dwRefData;

    switch (uMsg)
    {
    case WM_DESTROY:
        RemoveWindowSubclass(hwnd, PhpThemeSubclassWndProc, uIdSubclass);
        break;
    case WM_NOTIFY:
        {
            LPNMHDR data = (LPNMHDR)lParam;

            switch (data->code)
            {
            case NM_CUSTOMDRAW:
                {
                    SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PhSipDialogThemeDrawButton((LPNMTVCUSTOMDRAW)lParam));
                    return TRUE;
                }
                break;
            }
        }
        break;
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        {
            SetBkMode((HDC)wParam, TRANSPARENT);

            switch (PhCsGraphColorMode)
            {
            case 0: // New colors
                SetTextColor((HDC)wParam, RGB(0x0, 0x0, 0x0));
                SetDCBrushColor((HDC)wParam, RGB(0xff, 0xff, 0xff));
                break;
            case 1: // Old colors
                SetTextColor((HDC)wParam, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor((HDC)wParam, RGB(30, 30, 30));
                break;
            }

            return (INT_PTR)GetStockObject(DC_BRUSH);
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

BOOL CALLBACK PhpThemeWindowEnumWindowsProc(
    _In_ HWND WindowHandle,
    _In_ LPARAM lParam
    )
{
    //PPH_SYSINFO_SECTION context = (PPH_SYSINFO_SECTION)lParam;
    WCHAR className[256];

    if (!GetClassName(WindowHandle, className, ARRAYSIZE(className)))
        className[0] = 0;

    if (PhEqualStringZ(className, L"#32770", FALSE))
    {
        SetWindowSubclass(
            WindowHandle, 
            PhpThemeSubclassWndProc,
            0, 
            0//(ULONG_PTR)context
            );

        PhEnumChildWindows(
            WindowHandle, 
            0x1000, 
            PhpThemeWindowEnumWindowsProc,
            0//(LPARAM)context
            );
    } 
    else if (PhEqualStringZ(className, L"Button", FALSE))
    {
        //ULONG windowStyle = (ULONG)GetWindowLongPtr(PhSipWindow, GWL_STYLE);
    }

    return TRUE;
}

VOID PhThemeInitializeWindow(HWND WindowHandle)
{
    SetWindowSubclass(
        WindowHandle,
        PhpThemeSubclassWndProc,
        0,
        0//(ULONG_PTR)Section
        );

    PhEnumChildWindows(
        WindowHandle,
        0x1000,
        PhpThemeWindowEnumWindowsProc,
        0//(LPARAM)Section
        );
}



void SwapBlt(HDC hdc1, int x1, int y1, int nWidth, int nHeight, HDC hdc2, int x2, int y2)
{
    if(!nWidth || !nHeight) return;
    StretchBlt(hdc1, x1, y1, nWidth, nHeight,
               hdc2, x2, y2 + nHeight - 1, nWidth, -nHeight, SRCINVERT);
    StretchBlt(hdc2, x2, y2, nWidth, nHeight,
               hdc1, x1, y1 + nHeight - 1, nWidth, -nHeight, SRCINVERT);
    StretchBlt(hdc1, x1, y1, nWidth, nHeight,
               hdc2, x2, y2 + nHeight - 1, nWidth, -nHeight, SRCINVERT);
}


LRESULT CALLBACK TabProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    static BOOL bHover = FALSE;

    switch (uMsg)
    {
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

            InvalidateRect(hWnd, NULL, FALSE);

            if (!bHover)
            {
                TRACKMOUSEEVENT tmEvent = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&tmEvent);
                bHover = TRUE;
            }
        }
        break;
    case WM_MOUSELEAVE:
        {
            TCITEM tcItem = { TCIF_STATE, 0, TCIS_HIGHLIGHTED };

            int nCount = TabCtrl_GetItemCount(hWnd);

            for (int i = 0; i < nCount; i++)
            {
                TabCtrl_SetItem(hWnd, i, &tcItem);
            }

            InvalidateRect(hWnd, NULL, FALSE);
            bHover = FALSE;
        }
        break;
    case WM_PAINT:
        {
            HTHEME hTheme = OpenThemeData(hWnd, L"TAB");

            WCHAR szText[MAX_PATH + 1];
            RECT rcClient, rcItem;
            TCITEMW tcItem;
            PAINTSTRUCT ps;

            //InvalidateRect(hWnd, NULL, FALSE);

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
            SelectObject(hdc, PhApplicationFont);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
            SetDCBrushColor(hdc, RGB(65, 65, 65)); //RGB(28, 28, 28)); // RGB(65, 65, 65));
            FillRect(hdc, &rcClient, GetStockObject(DC_BRUSH));

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
                    SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                    SetDCBrushColor(hdc, RGB(128, 128, 128));

                    OffsetRect(&rcItem, 2, 2);
                    FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                    //FrameRect(hdc, &rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
                }
                else
                {
                    SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                    SetDCBrushColor(hdc, RGB(64, 64, 64));

                    OffsetRect(&rcItem, 2, 2);
                    FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                }

                if (tcItem.iImage >= 0)
                {
                    //HIMAGELIST hImageList = TabCtrl_GetImageList(hWnd);
                    //RECT rcIcon =
                    //{
                    //    rcItem.left + GetSystemMetrics(SM_CXSMICON) / 2,
                    //    rcItem.top,
                    //    rcIcon.left + GetSystemMetrics(SM_CXSMICON),
                    //    rcItem.top + GetSystemMetrics(SM_CYSMICON),
                    //};
                    //DrawThemeIcon(hTheme, hdc, TABP_TABITEM, TIS_NORMAL, &rcIcon, hImageList, tcItem.iImage);
                    //rcItem.left = rcIcon.right;
                }

                DrawText(
                    hdc,
                    tcItem.pszText,
                    -1,
                    &rcItem,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX //| DT_WORD_ELLIPSIS
                    );
            }

            int selectedIndex = TabCtrl_GetCurSel(hWnd);
            if (selectedIndex >= 0)
            {
                TabCtrl_GetItemRect(hWnd, selectedIndex, &rcItem);
                TabCtrl_GetItem(hWnd, selectedIndex, &tcItem);

                SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                SetDCBrushColor(hdc, RGB(30, 30, 30));

                OffsetRect(&rcItem, 2, 2);
                FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));

                if (tcItem.iImage >= 0)
                {
                    //HIMAGELIST hImageList = TabCtrl_GetImageList(hWnd);
                    //RECT rcIcon =
                    //{
                    //    rcItem.left + GetSystemMetrics(SM_CXSMICON) / 2,
                    //    rcItem.top,
                    //    rcItem.left + GetSystemMetrics(SM_CXSMICON),
                    //    rcItem.top + GetSystemMetrics(SM_CYSMICON),
                    //};
                    //DrawThemeIcon(hTheme, hdc, TABP_TABITEM, TIS_SELECTED, &rcIcon, hImageList, tcItem.iImage);
                }

                DrawText(
                    hdc,
                    tcItem.pszText,
                    -1,
                    &rcItem,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX
                    );
            }

            DeleteDC(hdcTab);

            BitBlt(ps.hdc, 0, 0, rcClient.right, rcClient.bottom, hdc, 0, 0, SRCCOPY);

            DeleteDC(hdc);
            DeleteObject(hbm);
            EndPaint(hWnd, &ps);
            CloseThemeData(hTheme);
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


VOID PhThemeInitializeTabWindow(HWND TabWindowHandle)
{
    SetWindowSubclass(TabWindowHandle, TabProc, 0, 0);
}

VOID PhThemeWindowDrawItem(PDRAWITEMSTRUCT DrawInfo)
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

VOID PhThemeWindowMeasureItem(PMEASUREITEMSTRUCT DrawInfo)
{
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

                hdc = GetDC(PhMainWndHandle);

                SelectObject(hdc, (HFONT)SendMessage(PhMainWndHandle, WM_GETFONT, 0, 0));

                GetTextExtentPoint32(hdc, menuItemInfo->Text, (INT)wcslen(menuItemInfo->Text), &size);

                DrawInfo->itemWidth = size.cx + 90;
                //drawInfo->itemHeight = 20;// size.cy;

                DeleteDC(hdc);
            }
        }
    }
}



LRESULT CALLBACK PhpHeaderSubclassCallback(
    HWND hWnd,
    UINT uMsg, 
    WPARAM wParam, 
    LPARAM lParam, 
    UINT_PTR uIdSubclass, 
    DWORD_PTR dwRefData
    )
{
    static BOOL bHover = FALSE;

    switch (uMsg)
    {
    //case WM_ERASEBKGND:
    //    return 1;
    //case WM_MOUSEMOVE:
    //    {
    //        POINT pt;
    //        RECT rcItem;
    //        TCITEM tcItem;
    //        tcItem.mask = TCIF_STATE;
    //        tcItem.dwStateMask = TCIS_HIGHLIGHTED;
    //        GetCursorPos(&pt);
    //        MapWindowPoints(NULL, hWnd, &pt, 1);
    //        int nCount = TabCtrl_GetItemCount(hWnd);
    //        for (int i = 0; i < nCount; i++)
    //        {
    //            TabCtrl_GetItemRect(hWnd, i, &rcItem);
    //            tcItem.dwState = PtInRect(&rcItem, pt) ? TCIS_HIGHLIGHTED : 0;
    //            TabCtrl_SetItem(hWnd, i, &tcItem);
    //        }
    //        InvalidateRect(hWnd, NULL, FALSE);
    //        if (!bHover)
    //        {
    //            TRACKMOUSEEVENT tmEvent = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
    //            TrackMouseEvent(&tmEvent);
    //            bHover = TRUE;
    //        }
    //    }
    //    break;
    //case WM_MOUSELEAVE:
    //    {
    //       // TCITEM tcItem = { TCIF_STATE, 0, TCIS_HIGHLIGHTED };
    //       //// int nCount = TabCtrl_GetItemCount(hWnd);
    //       // for (int i = 0; i < nCount; i++)
    //       // {
    //       //     //TabCtrl_SetItem(hWnd, i, &tcItem);
    //       // }
    //        InvalidateRect(hWnd, NULL, FALSE);
    //        bHover = FALSE;
    //    }
    //    break;
    case WM_PAINT:
        {
            HTHEME hTheme = OpenThemeData(hWnd, L"HEADER");

            WCHAR szText[MAX_PATH + 1];
            RECT rcClient, rcItem;
            HDITEM tcItem;
            PAINTSTRUCT ps;

            InvalidateRect(hWnd, NULL, FALSE);

            BeginPaint(hWnd, &ps);

            GetClientRect(hWnd, &rcClient);

            //Header_GetItemRect(hWnd, 0, &rcItem);

            ZeroMemory(&tcItem, sizeof(tcItem));
            tcItem.mask = HDI_TEXT | HDI_IMAGE;
            //tcItem.dwStateMask = TCIS_BUTTONPRESSED | TCIS_HIGHLIGHTED;
            tcItem.pszText = szText;
            tcItem.cchTextMax = MAX_PATH;

            HDC hdc = CreateCompatibleDC(ps.hdc);
            HBITMAP hbm = CreateCompatibleBitmap(ps.hdc, rcClient.right, rcClient.bottom);

            SelectObject(hdc, hbm);
            SelectObject(hdc, PhApplicationFont);

            SetBkMode(hdc, TRANSPARENT);
            //SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
            SetDCBrushColor(hdc, RGB(65, 65, 65)); //RGB(28, 28, 28)); // RGB(65, 65, 65));
            FillRect(hdc, &rcClient, GetStockObject(DC_BRUSH));

            HDC hdcTab = CreateCompatibleDC(hdc);

            int nCount = Header_GetItemCount(hWnd);
            for (int i = 0; i < nCount; i++)
            {
                Header_GetItemRect(hWnd, i, &rcItem);
                Header_GetItem(hWnd, i, &tcItem);

                POINT pt;
                GetCursorPos(&pt);
                MapWindowPoints(NULL, hWnd, &pt, 1);

                if (PtInRect(&rcItem, pt))
                {
                    SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                    SetDCBrushColor(hdc, RGB(128, 128, 128));
                    FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                    //FrameRect(hdc, &rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
                }
                else
                {
                    SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                    SetDCBrushColor(hdc, RGB(64, 64, 64));
                    FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));
                    //FrameRect(hdc, &rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
                }

                if (tcItem.iImage >= 0)
                {
                    //HIMAGELIST hImageList = TabCtrl_GetImageList(hWnd);
                    //RECT rcIcon =
                    //{
                    //    rcItem.left + GetSystemMetrics(SM_CXSMICON) / 2,
                    //    rcItem.top,
                    //    rcIcon.left + GetSystemMetrics(SM_CXSMICON),
                    //    rcItem.top + GetSystemMetrics(SM_CYSMICON),
                    //};
                    //DrawThemeIcon(hTheme, hdc, TABP_TABITEM, TIS_NORMAL, &rcIcon, hImageList, tcItem.iImage);
                    //rcItem.left = rcIcon.right;
                }

                DrawText(
                    hdc,
                    tcItem.pszText,
                    -1,
                    &rcItem,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX //| DT_WORD_ELLIPSIS
                    );
            }

            int selectedIndex = TabCtrl_GetCurSel(hWnd);
            //if (selectedIndex >= 0)
            {
                TabCtrl_GetItemRect(hWnd, selectedIndex, &rcItem);
                TabCtrl_GetItem(hWnd, selectedIndex, &tcItem);

                //SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
                //SetDCBrushColor(hdc, RGB(64, 64, 64));
                //FillRect(hdc, &rcItem, GetStockObject(DC_BRUSH));

                if (tcItem.iImage >= 0)
                {
                    //HIMAGELIST hImageList = TabCtrl_GetImageList(hWnd);
                    //RECT rcIcon =
                    //{
                    //    rcItem.left + GetSystemMetrics(SM_CXSMICON) / 2,
                    //    rcItem.top,
                    //    rcItem.left + GetSystemMetrics(SM_CXSMICON),
                    //    rcItem.top + GetSystemMetrics(SM_CYSMICON),
                    //};
                    //DrawThemeIcon(hTheme, hdc, TABP_TABITEM, TIS_SELECTED, &rcIcon, hImageList, tcItem.iImage);
                }

                DrawText(
                    hdc,
                    tcItem.pszText,
                    -1,
                    &rcItem,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX
                    );
            }

            DeleteDC(hdcTab);

            BitBlt(ps.hdc, 0, 0, rcClient.right, rcClient.bottom, hdc, 0, 0, SRCCOPY);

            DeleteDC(hdc);
            DeleteObject(hbm);
            EndPaint(hWnd, &ps);
            CloseThemeData(hTheme);
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


VOID PhThemeInitializeHeaderControl(HWND HeaderWindow)
{
    SetWindowSubclass(HeaderWindow, PhpHeaderSubclassCallback, 0, 0);
}