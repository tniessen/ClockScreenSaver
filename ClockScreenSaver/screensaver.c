#include <Windows.h>
#include <Commctrl.h>
#include <Scrnsave.h>
#include <ShlObj.h>
#include "resource.h"
#include "properties.h"
#include "settings.h"

#ifdef UNICODE
#pragma comment(lib, "ScrnSavw.lib")
#else
#pragma comment(lib, "ScrnSave.lib")
#endif

#pragma comment(lib, "comctl32.lib")

extern HINSTANCE hMainInstance;

PWSTR GetConfigPath() {
	// Get path to AppData/local
	PWSTR dir;
	if (SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &dir) != S_OK) {
		return NULL;
	}

	// File extension
	PWSTR ext = TEXT(".properties");

	// Construct full path to config file
	size_t n = wcslen(dir) + 1 + wcslen(szAppName) + wcslen(ext) + 1;
	PWSTR path = calloc(n, sizeof(WCHAR));
	if (!path) return NULL;

	wcscpy_s(path, n, dir);
	wcscat_s(path, n, TEXT("\\"));
	wcscat_s(path, n, szAppName);
	wcscat_s(path, n, ext);

	CoTaskMemFree(dir);

	return path;
}

static BOOL LoadConfig(PPROPERTIES props) {
	SecureZeroMemory(props, sizeof(PROPERTIES));

	PWSTR path = GetConfigPath();
	if (!path) return FALSE;

	return ReadProperties(props, path);
}

static BOOL SaveConfig(PPROPERTIES props) {
	PWSTR path = GetConfigPath();
	if (!path) return FALSE;

	return WriteProperties(props, path);
}

static BOOL LoadSettings(PPROPERTIES props, PSETTINGS settings) {
	// Load properties
	if (!LoadConfig(props)) {
		return FALSE;
	}

	// Extract settings
	PropertiesToSettings(settings, props);
	return TRUE;
}

// Loads settings or uses defaults if the configuration file does not exist.
static BOOL LoadSettingsOrUseDefaults(PPROPERTIES props, PSETTINGS settings) {
	if (LoadSettings(props, settings)) {
		return TRUE;
	}
	else if (GetLastError() == ERROR_FILE_NOT_FOUND) {
		RestoreDefaultSettings(settings);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

static BOOL SaveSettings(PPROPERTIES props, PSETTINGS settings) {
	SettingsToProperties(settings, props);
	return SaveConfig(props);
}

void UpdateCustomFont(HWND hCheckbox, HWND hButton, HWND hLabel, PSETTINGS settings) {
	LRESULT state = SendMessage(hCheckbox, BM_GETCHECK, 0, 0);
	settings->useCustomFont = (state == BST_CHECKED);
	EnableWindow(hButton, settings->useCustomFont);
	EnableWindow(hLabel, settings->useCustomFont);
}

UINT_PTR CALLBACK ChooseFontHook(HWND hDlg, UINT uiMsg, WPARAM wparam, LPARAM lparam) {
	if (uiMsg == WM_INITDIALOG) {
		// On Win 10 Pro x64, the font size selection has the id 1138
		HWND hFontSize = GetDlgItem(hDlg, 1138);
		if (hFontSize) {
			EnableWindow(hFontSize, FALSE);
		}
	}
	return 0;
}

static void CreateLFont(PLOGFONT font, PWSTR name, UINT height, UINT weight, BOOL italic) {
	ZeroMemory(font, sizeof(LOGFONT));
	font->lfHeight = height;
	font->lfWeight = weight;
	font->lfItalic = italic;
	font->lfCharSet = ANSI_CHARSET;
	font->lfOutPrecision = OUT_OUTLINE_PRECIS;
	font->lfClipPrecision = CLIP_DEFAULT_PRECIS;
	font->lfQuality = CLEARTYPE_QUALITY;
	font->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcscpy_s(font->lfFaceName, 32, name);
}

static void RectToSize(PRECT rect, PSIZE size) {
	size->cx = rect->right - rect->left;
	size->cy = rect->bottom - rect->top;
}

static void InvalidateWindow(HWND hWindow) {
	RECT rect;
	GetClientRect(hWindow, &rect);
	InvalidateRect(hWindow, &rect, TRUE);
}

static BOOL ChooseCustomFont(HWND hDlg, PSETTINGS settings) {
	LOGFONT lFont;
	CreateLFont(&lFont, settings->fontName, 20, settings->fontWeight, settings->fontItalic);

	CHOOSEFONT cFont;
	ZeroMemory(&cFont, sizeof(cFont));
	cFont.lStructSize = sizeof(cFont);
	cFont.hwndOwner = hDlg;
	cFont.lpLogFont = &lFont;
	cFont.rgbColors = RGB(255, 255, 255);
	cFont.Flags = CF_ENABLEHOOK | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL;
	cFont.lpfnHook = ChooseFontHook;

	if (ChooseFont(&cFont)) {
		settings->fontName = _wcsdup(lFont.lfFaceName);
		settings->fontWeight = lFont.lfWeight;
		settings->fontItalic = lFont.lfItalic;
		return TRUE;
	}

	return FALSE;
}

static BOOL ChooseCustomColor(HWND hDlg, LPCOLORREF color) {
	static COLORREF customColors[16];
	CHOOSECOLOR cColor;

	ZeroMemory(&cColor, sizeof(cColor));
	cColor.lStructSize = sizeof(cColor);
	cColor.hwndOwner = hDlg;
	cColor.rgbResult = *color;
	cColor.Flags = CC_FULLOPEN | CC_RGBINIT;
	cColor.lpCustColors = customColors;

	if (ChooseColor(&cColor)) {
		*color = cColor.rgbResult;
		return TRUE;
	}

	return FALSE;
}

static int ErrorMessageBox(HWND hWnd, PWSTR caption, UINT btnType) {
	WCHAR msg[500];
	wsprintf(msg, TEXT("Error code: %lu\n"), GetLastError());

	return MessageBox(hWnd, msg, caption, MB_ICONERROR | btnType);
}

static void CenterWindowOnDesktop(HWND hwnd) {
	// Obtain rects
	RECT desktopRect, windowRect;
	GetClientRect(GetDesktopWindow(), &desktopRect);
	GetClientRect(hwnd, &windowRect);

	// Obtain sizes
	SIZE desktopSize, windowSize;
	RectToSize(&desktopRect, &desktopSize);
	RectToSize(&windowRect, &windowSize);

	// Calculate new position
	int x = (desktopSize.cx - windowSize.cx) / 2;
	int y = (desktopSize.cy - windowSize.cy) / 2;

	// Apply position
	SetWindowPos(hwnd, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static PROPERTIES properties;
	static SETTINGS settings;

	static HWND hScale;       // handle to scale track bar
	static HWND hSpace;       // handle to space track bar
	static HWND hSeconds;     // handle to "show seconds" checkbox
	static HWND hFontCheck;   // handle to "use custom font" checkbox
	static HWND hFontButton;  // handle to font button
	static HWND hCurrentFont; // handle to current font label

	// Button brushes
	static HBRUSH hFgBrush = NULL, hBgBrush = NULL;

	int buttonId;

	switch (message) {
	case WM_INITDIALOG:
		// Enable styling
		InitCommonControls();

		// Retrieve the application name from the .rc file.
		LoadString(hMainInstance, idsAppName, szAppName, APPNAMEBUFFERLEN);

		// Load settings
		while (!LoadSettingsOrUseDefaults(&properties, &settings)) {
			switch (ErrorMessageBox(hDlg, L"Failed to load configuration", MB_ABORTRETRYIGNORE)) {
			case IDRETRY:
				continue;
			case IDIGNORE:
				RestoreDefaultSettings(&settings);
				goto ignoreLoadSettingsFailure;
			default:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
		}

	ignoreLoadSettingsFailure:

		// Initialize the scale control
		hScale = GetDlgItem(hDlg, IDC_SCALE);
		SendMessage(hScale, TBM_SETTICFREQ, 10, 0);
		SendMessage(hScale, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
		SendMessage(hScale, TBM_SETPOS, TRUE, settings.scale);

		hSpace = GetDlgItem(hDlg, IDC_SPACE);
		SendMessage(hSpace, TBM_SETTICFREQ, 10, 0);
		SendMessage(hSpace, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
		SendMessage(hSpace, TBM_SETPOS, TRUE, settings.space);

		hSeconds = GetDlgItem(hDlg, IDC_SECONDS);
		SendMessage(hSeconds, BM_SETCHECK, settings.showSeconds ? BST_CHECKED : BST_UNCHECKED, 0);

		hFontCheck = GetDlgItem(hDlg, IDC_CUSTOM_FONT);
		SendMessage(hFontCheck, BM_SETCHECK, settings.useCustomFont ? BST_CHECKED : BST_UNCHECKED, 0);

		hFontButton = GetDlgItem(hDlg, IDC_CHOOSE_FONT);

		hCurrentFont = GetDlgItem(hDlg, IDC_CURRENT_FONT);
		SetWindowText(hCurrentFont, settings.fontName);

		UpdateCustomFont(hFontCheck, hFontButton, hCurrentFont, &settings);

		hFgBrush = CreateSolidBrush(settings.fgColor);
		hBgBrush = CreateSolidBrush(settings.bgColor);

		// If the configuration dialog was not opened relative to another window, center it on the screen
		if (GetParent(hDlg) == 0) {
			CenterWindowOnDesktop(hDlg);
		}

		return TRUE;
	case WM_COMMAND:
		buttonId = LOWORD(wParam);

		switch (buttonId) {
		case IDC_CHOOSE_FONT:
			// Show font dialog
			if (ChooseCustomFont(hDlg, &settings)) {
				SetWindowText(hCurrentFont, settings.fontName);
			}
			return TRUE;
		case IDC_FOREGROUND:
			// Show color picker for foreground
			if (ChooseCustomColor(hDlg, &settings.fgColor)) {
				hFgBrush = CreateSolidBrush(settings.fgColor);
				InvalidateWindow(hDlg);
			}
			return TRUE;
		case IDC_BACKGROUND:
			// Show color picker for background
			if (ChooseCustomColor(hDlg, &settings.bgColor)) {
				hBgBrush = CreateSolidBrush(settings.bgColor);
				InvalidateWindow(hDlg);
			}
			return TRUE;
		case IDC_CUSTOM_FONT:
			// Toggle custom font
			UpdateCustomFont(hFontCheck, hFontButton, hCurrentFont, &settings);
			return TRUE;
		case IDC_SECONDS:
			settings.showSeconds = IsDlgButtonChecked(hDlg, IDC_SECONDS);
			return TRUE;
		case IDC_RESTORE_DEFAULTS:
			// Restore settings
			RestoreDefaultSettings(&settings);

			// Update all controls
			SendMessage(hScale, TBM_SETPOS, FALSE, settings.scale);
			SendMessage(hSpace, TBM_SETPOS, FALSE, settings.space);
			SendMessage(hSeconds, BM_SETCHECK, settings.showSeconds ? BST_CHECKED : BST_UNCHECKED, 0);
			SendMessage(hFontCheck, BM_SETCHECK, settings.useCustomFont ? BST_CHECKED : BST_UNCHECKED, 0);
			SetWindowText(hCurrentFont, settings.fontName);
			hFgBrush = CreateSolidBrush(settings.fgColor);
			hBgBrush = CreateSolidBrush(settings.bgColor);

			// Ensure that all visuals are updated
			UpdateCustomFont(hFontCheck, hFontButton, hCurrentFont, &settings);
			InvalidateWindow(hDlg);

			return TRUE;
		case IDC_OK:
			// Save settings
			while (!SaveSettings(&properties, &settings)) {
				switch (ErrorMessageBox(hDlg, L"Failed to save configuration", MB_ABORTRETRYIGNORE)) {
				case IDRETRY:
					continue;
				case IDIGNORE:
					goto ignoreSaveSettingsFailure;
				default:
					return TRUE;
				}
			}

		ignoreSaveSettingsFailure:

		case IDC_CANCEL:
			EndDialog(hDlg, LOWORD(wParam) == IDC_OK);
			return TRUE;
		}
		break;
	case WM_HSCROLL:
		// Track bar notifications
		switch (LOWORD(wParam)) {
		case TB_ENDTRACK:
			switch (GetDlgCtrlID((HWND)lParam)) {
			case IDC_SCALE:
				settings.scale = SendMessage(hScale, TBM_GETPOS, 0, 0);
				return TRUE;
			case IDC_SPACE:
				settings.space = SendMessage(hSpace, TBM_GETPOS, 0, 0);
				return TRUE;
			}
			break;
		}
		break;
	case WM_CTLCOLORBTN:
		// Button colors
		buttonId = GetDlgCtrlID((HWND)lParam);
		switch (buttonId) {
		case IDC_FOREGROUND:
			if (hFgBrush) {
				return (INT_PTR)hFgBrush;
			}
			break;
		case IDC_BACKGROUND:
			if (hBgBrush) {
				return (INT_PTR)hBgBrush;
			}
			break;
		}
		break;
	case WM_CLOSE:
		// Close dialog
		EndDialog(hDlg, FALSE);
		return TRUE;
	}

	return FALSE;
}

BOOL WINAPI RegisterDialogClasses(HANDLE hInst) {
	return TRUE;
}

HANDLE AddFontFromResource(PWSTR resID, DWORD *installed) {
	HMODULE hMod = GetModuleHandle(NULL);

	*installed = 0;

	// Locate the resource
	HRSRC res = FindResource(hMod, resID, RT_FONT);
	if (!res) {
		return NULL;
	}

	// Retrieve its size
	DWORD length = SizeofResource(hMod, res);
	if (length == 0 && GetLastError() != ERROR_SUCCESS) {
		return NULL;
	}

	// Retrieve handle to resource
	HGLOBAL resAddr = LoadResource(hMod, res);
	if (!resAddr) {
		return NULL;
	}

	// Retrieve pointer to resource data
	PVOID resData = LockResource(resAddr);
	if (!resData) {
		return NULL;
	}

	// Add the font using the newly retrieved pointer
	return AddFontMemResourceEx(resData, length, 0, installed);
}

static void IntToTwoDigits(WORD w, PWSTR out) {
	out[0] = '0' + (w / 10);
	out[1] = '0' + (w % 10);
	out[2] = '\0';
}

static HANDLE CreateClockFont(UINT size, PSETTINGS settings, PWSTR defFontName) {
	PWSTR fontName;
	UINT weight;
	BOOL italic;

	if (settings->useCustomFont && settings->fontName) {
		fontName = settings->fontName;
		weight = settings->fontWeight;
		italic = settings->fontItalic;
	}
	else {
		fontName = defFontName;
		weight = FW_DONTCARE;
		italic = FALSE;
	}

	LOGFONT lfont;
	CreateLFont(&lfont, fontName, size, weight, italic);

	return CreateFontIndirect(&lfont);
}

LRESULT WINAPI ScreenSaverProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	// These static variables will be initialized at WM_CREATE
	static UINT         uTimer;
	static HANDLE       hDefaultFont;
	static WCHAR        defaultFontName[32];
	static PROPERTIES   properties;
	static SETTINGS     settings;
	static HBRUSH       hBgBrush;

	// Other local variables which do not need to be preserved
	HDC                 hdc;
	RECT                rc;

	switch (message) {
	case WM_CREATE:
		// Retrieve the application name from the .rc file.
		LoadString(hMainInstance, idsAppName, szAppName, APPNAMEBUFFERLEN);

		// Load settings
		if (!LoadSettingsOrUseDefaults(&properties, &settings)) {
			ErrorMessageBox(hwnd, L"Failed to load configuration", MB_OK);
			DestroyWindow(hwnd);
			return TRUE;
		}

		DWORD nFontsInstalled;
		hDefaultFont = AddFontFromResource(MAKEINTRESOURCE(ID_DEFAULT_FONT_FILE), &nFontsInstalled);
		LoadString(hMainInstance, IDS_DEFAULT_FONT_NAME, defaultFontName, 32);

		// Background brush
		hBgBrush = CreateSolidBrush(settings.bgColor);

		// Set a timer for the screen saver window.
		uTimer = SetTimer(hwnd, 1, 200, NULL);

		break;
	case WM_ERASEBKGND:
		// The WM_ERASEBKGND message is issued before the
		// WM_TIMER message, allowing the screen saver to
		// paint the background as appropriate.
		hdc = GetDC(hwnd);
		GetClientRect(hwnd, &rc);
		FillRect(hdc, &rc, hBgBrush);
		ReleaseDC(hwnd, hdc);

		return TRUE;
	case WM_TIMER:
		// First, retrieve the device context
		hdc = GetDC(hwnd);
		// and the associated client area
		GetClientRect(hwnd, &rc);

		// Obtain the size of the client area
		SIZE szrc;
		RectToSize(&rc, &szrc);

		// Start double-buffering
		HDC memhdc = CreateCompatibleDC(hdc);
		HBITMAP membitmap = CreateCompatibleBitmap(hdc, szrc.cx, szrc.cy);
		SelectObject(memhdc, membitmap);

		// Paint background
		FillRect(memhdc, &rc, hBgBrush);

		// Retrieve the current time
		SYSTEMTIME time;
		GetLocalTime(&time);

		// Number of units to display
		UINT nUnits = settings.showSeconds ? 3 : 2;

		// Generate text blocks
		WCHAR units[3][3];
		IntToTwoDigits(time.wHour, units[0]);
		IntToTwoDigits(time.wMinute, units[1]);
		IntToTwoDigits(time.wSecond, units[2]);

		// Layout parameters
		int availableWidth = szrc.cx * settings.scale / 100;
		int widthPerUnit = availableWidth / nUnits;
		int textWidthPerUnit = widthPerUnit * (100 - settings.space) / 100;
		LONG marginX = szrc.cx * (100 - settings.scale) / 2 / 100;
		LONG offsetX = rc.left + marginX;

		// Ensure that logic units map to pixels
		SetMapMode(memhdc, MM_TEXT);

		// Create a font with maximal height
		HANDLE hFont = CreateClockFont(szrc.cy, &settings, defaultFontName);
		SelectObject(memhdc, hFont);

		// Measure how big the text will be
		SIZE textSize;
		GetTextExtentPoint32(memhdc, TEXT("00"), 2, &textSize);

		// Calculate how to scale the text
		float f = max((float)textSize.cx / textWidthPerUnit, 1);
		int newTextSize = (int)(szrc.cy / f);

		// Replace the font with a new object with the correct size
		DeleteObject(hFont);
		hFont = CreateClockFont(newTextSize, &settings, defaultFontName);
		SelectObject(memhdc, hFont);

		// Prepare text drawing
		SetTextColor(memhdc, settings.fgColor);
		SetBkColor(memhdc, settings.bgColor);
		SetBkMode(memhdc, OPAQUE);

		// Draw all units
		for (UINT i = 0; i < nUnits; i++) {
			RECT rect = {
				.left = offsetX + widthPerUnit * i,
				.right = offsetX + widthPerUnit * (i + 1),
				.top = rc.top,
				.bottom = rc.bottom
			};
			DrawText(memhdc, units[i], 2, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		}

		// End double-buffering
		BitBlt(hdc, rc.left, rc.top, szrc.cx, szrc.cy, memhdc, 0, 0, SRCCOPY);
		DeleteObject(membitmap);
		DeleteDC(memhdc);

		// End drawing
		ReleaseDC(hwnd, hdc);

		return TRUE;
	case WM_DESTROY:
		// Destroy our timer
		if (uTimer) {
			KillTimer(hwnd, uTimer);
		}

		break;
	}

	// DefScreenSaverProc processes any messages ignored by ScreenSaverProc.
	return DefScreenSaverProc(hwnd, message, wParam, lParam);
}
