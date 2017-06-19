#ifndef _PH_PHTHEME_H
#define _PH_PHTHEME_H

#include <aclui.h>

// begin_phapppub
PHLIBAPI
VOID
NTAPI
PhThemeInitializeWindow(
    _In_ HWND WindowHandle
    );

PHLIBAPI
VOID
NTAPI
PhThemeReInitializeWindow(
    _In_ HWND WindowHandle
    );

PHLIBAPI
VOID
NTAPI
PhThemeInitializeTabWindow(
    _In_ HWND TabWindowHandle
    );

PHLIBAPI
VOID
NTAPI
PhThemeInitializeHeaderControl(
    _In_ HWND HeaderWindow
    );

PHLIBAPI
VOID
NTAPI
PhThemeInitializeStatusbarControl(
    _In_ HWND HeaderWindow
    );

PHLIBAPI
LRESULT
NTAPI
PhThemeDrawButton(
    _In_ LPNMCUSTOMDRAW drawInfo
    );

PHLIBAPI
BOOLEAN
NTAPI
PhThemeWindowDrawItem(
    _In_ PDRAWITEMSTRUCT DrawInfo
    );

PHLIBAPI
BOOLEAN
NTAPI
PhThemeWindowMeasureItem(
    _In_ PMEASUREITEMSTRUCT DrawInfo
    );

PHLIBAPI
LRESULT
NTAPI
PhThemeWindowDrawRebar(
    _In_ LPNMCUSTOMDRAW DrawInfo
    );

PHLIBAPI
LRESULT
NTAPI
PhThemeWindowDrawToolbar(
    _In_ LPNMTBCUSTOMDRAW DrawInfo
    );
// end_phapppub

#endif
