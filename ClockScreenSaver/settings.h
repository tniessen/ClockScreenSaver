#pragma once

#include <Windows.h>
#include "properties.h"

typedef struct {
	UINT scale;
	UINT space;
	BOOL showSeconds;
	BOOL useCustomFont;
	PWSTR fontName;
	UINT fontWeight;
	BOOL fontItalic;
	COLORREF fgColor;
	COLORREF bgColor;
} SETTINGS, *PSETTINGS;

void PropertiesToSettings(PSETTINGS settings, PPROPERTIES props);

void SettingsToProperties(PSETTINGS settings, PPROPERTIES props);

void RestoreDefaultSettings(PSETTINGS settings);
