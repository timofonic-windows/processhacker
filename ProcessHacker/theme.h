#pragma once

// begin_phapppub
VOID PhThemeInitializeWindow(HWND WindowHandle);
VOID PhThemeInitializeTabWindow(HWND TabHandle);
VOID PhThemeWindowDrawItem(PDRAWITEMSTRUCT DrawInfo);
VOID PhThemeWindowMeasureItem(PMEASUREITEMSTRUCT DrawInfo);

VOID PhThemeInitializeHeaderControl(HWND HeaderWindow);
// end_phapppub